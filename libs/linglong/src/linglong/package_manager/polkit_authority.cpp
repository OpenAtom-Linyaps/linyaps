// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "polkit_authority.h"

#include "linglong/utils/log/log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QMap>
#include <QString>
#include <QVariant>

#include <mutex>

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

    PolkitSubject subject;
    subject.kind = QStringLiteral("system-bus-name");
    subject.details.insert(QStringLiteral("name"),
                           QVariant::fromValue(QString::fromStdString(systemBusName)));

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
