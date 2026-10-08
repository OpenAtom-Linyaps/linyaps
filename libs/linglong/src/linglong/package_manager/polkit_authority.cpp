// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "polkit_authority.h"

#include "linglong/utils/log/log.h"

#include <fmt/format.h>
#include <systemd/sd-login.h>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QMap>
#include <QString>
#include <QVariant>

#include <cstdlib>
#include <array>
#include <algorithm>
#include <mutex>
#include <string_view>

#include <sys/types.h>

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

constexpr const char *DBUS_SERVICE = "org.freedesktop.DBus";
constexpr const char *DBUS_PATH = "/org/freedesktop/DBus";
constexpr const char *DBUS_INTERFACE = "org.freedesktop.DBus";

} // namespace

Q_DECLARE_METATYPE(PolkitSubject)
Q_DECLARE_METATYPE(PolkitResult)

namespace linglong::service {

namespace {

// Reads a numeric property of the connection behind `systemBusName` from the
// bus daemon. Only the bus daemon knows the credentials of a connection, so the
// caller can not fake them.
utils::error::Result<quint32> queryBusConnection(const char *method,
                                                 const std::string &systemBusName)
{
    LINGLONG_TRACE("query a property of a dbus connection");

    auto msg = QDBusMessage::createMethodCall(QString::fromLatin1(DBUS_SERVICE),
                                              QString::fromLatin1(DBUS_PATH),
                                              QString::fromLatin1(DBUS_INTERFACE),
                                              QString::fromLatin1(method));
    msg << QString::fromStdString(systemBusName);

    // A stalled bus must not block the package manager dispatch thread forever.
    const QDBusReply<quint32> reply = QDBusConnection::systemBus().call(msg, QDBus::Block, 1000);
    if (!reply.isValid()) {
        return LINGLONG_ERR(
          fmt::format("{} failed: {}", method, reply.error().message().toStdString()));
    }

    return reply.value();
}

utils::error::Result<std::string> resolveCallerSession(const std::string &systemBusName)
{
    LINGLONG_TRACE("resolve the identity of a polkit caller");

    if (systemBusName.empty()) {
        return LINGLONG_ERR("the caller did not provide its bus name");
    }

    auto pid = queryBusConnection("GetConnectionUnixProcessID", systemBusName);
    if (!pid) {
        return LINGLONG_ERR(pid);
    }

    // The pid only serves as a lookup key for the login session: unlike the pid
    // the session stays the same while ll-cli is restarted for every operation.
    char *sessionId = nullptr;
    if (::sd_pid_get_session(static_cast<::pid_t>(*pid), &sessionId) < 0 || sessionId == nullptr) {
        return LINGLONG_ERR(
          fmt::format("the caller {} does not belong to a login session", systemBusName));
    }

    std::string session{ sessionId };
    ::free(sessionId);

    return session;
}

constexpr std::array<std::string_view, 4> keepActions{
    "org.deepin.linglong.PackageManager1.install",
    "org.deepin.linglong.PackageManager1.install-from-file",
    "org.deepin.linglong.PackageManager1.uninstall",
    "org.deepin.linglong.PackageManager1.update",
};

} // namespace

PolkitSubjectIdentity selectPolkitSubject(std::string_view actionId,
                                          std::string_view busName,
                                          std::string_view sessionId)
{
    if (!sessionId.empty()
        && std::find(keepActions.cbegin(), keepActions.cend(), actionId) != keepActions.cend()) {
        return { "unix-session", std::string{ sessionId } };
    }
    return { "system-bus-name", std::string{ busName } };
}

void PolkitAuthority::checkAuthorizationAsync(
  const std::string &actionId,
  const std::string &systemBusName,
  std::function<void(utils::error::Result<bool>)> callback,
  bool userInteraction)
{
    LINGLONG_TRACE("check polkit authorization");

    register_type();

    std::string sessionId;
    if (std::find(keepActions.cbegin(), keepActions.cend(), actionId) != keepActions.cend()) {
        auto resolved = resolveCallerSession(systemBusName);
        if (resolved) {
            sessionId = std::move(*resolved);
        } else {
            LogD("can't resolve the login session of {}: {}",
                 systemBusName,
                 resolved.error().message());
        }
    }

    auto bus = QDBusConnection::systemBus();

    auto msg = QDBusMessage::createMethodCall(QString::fromLatin1(POLKIT_SERVICE),
                                              QString::fromLatin1(POLKIT_PATH),
                                              QString::fromLatin1(POLKIT_INTERFACE),
                                              QStringLiteral("CheckAuthorization"));

    PolkitSubject subject;
    const auto selected = selectPolkitSubject(actionId, systemBusName, sessionId);
    subject.kind = QString::fromLatin1(selected.kind.data(), selected.kind.size());
    subject.details.insert(selected.kind == "unix-session" ? QStringLiteral("session-id")
                                                            : QStringLiteral("name"),
                           QVariant::fromValue(QString::fromStdString(selected.value)));

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
