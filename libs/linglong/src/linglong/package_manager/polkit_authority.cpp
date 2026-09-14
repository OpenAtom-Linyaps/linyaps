// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "polkit_authority.h"

#include "linglong/utils/log/log.h"

#include <systemd/sd-login.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMap>
#include <QString>
#include <QVariant>

#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>

namespace {

struct PolkitSubject
{
    QString kind;
    QVariantMap details;
};

struct PolkitResult
{
    bool isAuthorized = false;
    bool isChallenge = false;
    QMap<QString, QString> details;
};

QDBusArgument &operator<<(QDBusArgument &arg, const PolkitSubject &subject)
{
    arg.beginStructure();
    arg << subject.kind << subject.details;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PolkitSubject &subject)
{
    arg.beginStructure();
    arg >> subject.kind >> subject.details;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const PolkitResult &result)
{
    arg.beginStructure();
    arg << result.isAuthorized << result.isChallenge << result.details;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PolkitResult &result)
{
    arg.beginStructure();
    arg >> result.isAuthorized >> result.isChallenge >> result.details;
    arg.endStructure();
    return arg;
}

void register_type()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        qDBusRegisterMetaType<PolkitSubject>();
        qDBusRegisterMetaType<PolkitResult>();
        qDBusRegisterMetaType<QMap<QString, QString>>();
    });
}

constexpr const char *POLKIT_SERVICE = "org.freedesktop.PolicyKit1";
constexpr const char *POLKIT_PATH = "/org/freedesktop/PolicyKit1/Authority";
constexpr const char *POLKIT_INTERFACE = "org.freedesktop.PolicyKit1.Authority";

std::optional<QString> resolveSessionId(const QDBusConnection &bus, const QString &systemBusName)
{
    auto *interface = bus.interface();
    if (interface == nullptr) {
        LogW("failed to access the system bus interface");
        return std::nullopt;
    }

    const auto pidReply = interface->servicePid(systemBusName);
    if (!pidReply.isValid()) {
        LogW("failed to resolve PID for {}: {}",
             systemBusName.toStdString(),
             pidReply.error().message().toStdString());
        return std::nullopt;
    }

    char *rawSessionId = nullptr;
    const auto result = sd_pid_get_session(static_cast<pid_t>(pidReply.value()), &rawSessionId);
    std::unique_ptr<char, decltype(&std::free)> sessionId(rawSessionId, &std::free);
    if (result < 0) {
        LogD("failed to resolve login session for {}: {}", systemBusName.toStdString(), result);
        return std::nullopt;
    }

    // 唯一总线名不会被复用；再次核对 PID，避免调用方退出后发生 PID 复用竞态。
    const auto currentPidReply = interface->servicePid(systemBusName);
    if (!currentPidReply.isValid() || currentPidReply.value() != pidReply.value()) {
        LogW("caller {} disappeared while resolving its login session",
             systemBusName.toStdString());
        return std::nullopt;
    }

    return QString::fromUtf8(sessionId.get());
}

PolkitSubject makeSubject(const QDBusConnection &bus, const QString &systemBusName)
{
    PolkitSubject subject;
    if (const auto sessionId = resolveSessionId(bus, systemBusName); sessionId.has_value()) {
        // 会话级 subject 让 polkit 的 auth_admin_keep 缓存可在同一登录会话的新进程间复用。
        subject.kind = QStringLiteral("unix-session");
        subject.details.insert(QStringLiteral("session-id"), QVariant::fromValue(*sessionId));
        return subject;
    }

    // 系统服务等调用方可能不属于登录会话，保留原有的逐进程授权行为。
    subject.kind = QStringLiteral("system-bus-name");
    subject.details.insert(QStringLiteral("name"), QVariant::fromValue(systemBusName));
    return subject;
}

} // namespace

Q_DECLARE_METATYPE(PolkitSubject)
Q_DECLARE_METATYPE(PolkitResult)

namespace linglong::service {

void PolkitAuthority::checkAuthorizationAsync(
  const std::string &actionId,
  const std::string &systemBusName,
  std::function<void(utils::error::Result<bool>)> callback,
  bool userInteraction)
{
    LINGLONG_TRACE("check polkit authorization");

    register_type();

    auto bus = QDBusConnection::systemBus();

    auto msg = QDBusMessage::createMethodCall(QString::fromLatin1(POLKIT_SERVICE),
                                              QString::fromLatin1(POLKIT_PATH),
                                              QString::fromLatin1(POLKIT_INTERFACE),
                                              QStringLiteral("CheckAuthorization"));

    const auto subject = makeSubject(bus, QString::fromStdString(systemBusName));

    msg << QVariant::fromValue(subject) << QString::fromStdString(actionId)
        << QVariant::fromValue(QMap<QString, QString>())
        << static_cast<uint>(userInteraction ? 1 : 0) << QString();

    auto pendingCall = bus.asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(pendingCall);
    QObject::connect(
      watcher,
      &QDBusPendingCallWatcher::finished,
      [callback = std::move(callback)](QDBusPendingCallWatcher *self) {
          LINGLONG_TRACE("check polkit authorization");

          LogD("CheckAuthorization return");
          self->deleteLater();

          QDBusPendingReply<PolkitResult> res = *self;
          if (res.isError()) {
              callback(LINGLONG_ERR(
                fmt::format("polkit check failed: {}", res.error().message().toStdString())));
              return;
          }

          callback(res.value().isAuthorized);
      });
}

} // namespace linglong::service
