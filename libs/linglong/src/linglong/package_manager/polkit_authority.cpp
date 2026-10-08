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
#include <mutex>
#include <optional>

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

    // The system bus is a local socket, so this blocking round trip is cheap
    // and the bus daemon always answers.
    const QDBusReply<quint32> reply = QDBusConnection::systemBus().call(msg);
    if (!reply.isValid()) {
        return LINGLONG_ERR(
          fmt::format("{} failed: {}", method, reply.error().message().toStdString()));
    }

    return reply.value();
}

} // namespace

utils::error::Result<PolkitCallerIdentity>
PolkitAuthority::resolveCallerIdentity(const std::string &systemBusName) noexcept
{
    LINGLONG_TRACE("resolve the identity of a polkit caller");

    if (systemBusName.empty()) {
        return LINGLONG_ERR("the caller did not provide its bus name");
    }

    auto pid = queryBusConnection("GetConnectionUnixProcessID", systemBusName);
    if (!pid) {
        return LINGLONG_ERR(pid);
    }

    auto uid = queryBusConnection("GetConnectionUnixUser", systemBusName);
    if (!uid) {
        return LINGLONG_ERR(uid);
    }

    // The pid only serves as a lookup key for the login session: unlike the pid
    // the session stays the same while ll-cli is restarted for every operation.
    char *sessionId = nullptr;
    if (::sd_pid_get_session(static_cast<::pid_t>(*pid), &sessionId) < 0 || sessionId == nullptr) {
        return LINGLONG_ERR(
          fmt::format("the caller {} does not belong to a login session", systemBusName));
    }

    PolkitCallerIdentity identity{ .uid = *uid, .sessionId = sessionId };
    ::free(sessionId);

    return identity;
}

void PolkitAuthority::checkAuthorizationAsync(
  const std::string &actionId,
  const std::string &systemBusName,
  std::function<void(utils::error::Result<bool>)> callback,
  bool userInteraction)
{
    LINGLONG_TRACE("check polkit authorization");

    register_type();

    // polkit remembers the authorizations it grants for `auth_*_keep` actions
    // per subject, and a `system-bus-name` subject is resolved to the calling
    // process. Since ll-cli is started from scratch for every operation, that
    // key never repeats and the retained authorization can not be reused.
    // Remember the grant for the session of the caller instead, for the same
    // five minutes polkit itself would have kept it.
    std::optional<PolkitCallerIdentity> identity;
    if (isKeepAuthorizedAction(actionId)) {
        auto resolved = resolveCallerIdentity(systemBusName);
        if (resolved) {
            identity = std::move(*resolved);
        } else {
            // Without a session there is nothing to key the cache on, so fall
            // back to asking polkit for every single call.
            LogD("can't remember an authorization for {}: {}",
                 systemBusName,
                 resolved.error().message());
        }

        if (identity && PolkitAuthorizationCache::instance().isGranted(*identity, actionId)) {
            LogD("reuse the authorization of session {} for {}", identity->sessionId, actionId);
            callback(true);
            return;
        }
    }

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
      [actionId,
       identity = std::move(identity),
       callback = std::move(callback)](QDBusPendingCallWatcher *self) {
          LINGLONG_TRACE("check polkit authorization");

          LogD("CheckAuthorization return");
          self->deleteLater();

          QDBusPendingReply<PolkitResult> res = *self;
          if (res.isError()) {
              callback(LINGLONG_ERR(
                fmt::format("polkit check failed: {}", res.error().message().toStdString())));
              return;
          }

          const bool isAuthorized = res.value().isAuthorized;
          if (isAuthorized && identity) {
              // polkit only retains this authorization for the connection of the
              // caller, which is about to go away, so keep it for the session.
              PolkitAuthorizationCache::instance().grant(*identity, actionId);
          }

          callback(isAuthorized);
      });
}

} // namespace linglong::service
