// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "polkit_authority.h"

#include "linglong/utils/log/log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QMap>
#include <QString>
#include <QVariant>

#include <mutex>

struct PolkitSubject
{
    QString kind;
    QVariantMap details;
};

struct PolkitResult
{
    bool isAuthorized;
    bool isChallenge;
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

Q_DECLARE_METATYPE(PolkitSubject)
Q_DECLARE_METATYPE(PolkitResult)

namespace linglong::service {

namespace {

constexpr const char *POLKIT_SERVICE = "org.freedesktop.PolicyKit1";
constexpr const char *POLKIT_PATH = "/org/freedesktop/PolicyKit1/Authority";
constexpr const char *POLKIT_INTERFACE = "org.freedesktop.PolicyKit1.Authority";

} // namespace

utils::error::Result<bool> PolkitAuthority::checkAuthorization(const std::string &actionId,
                                                               const std::string &systemBusName,
                                                               bool userInteraction)
{
    LINGLONG_TRACE("check polkit authorization");

    static std::once_flag flag;
    std::call_once(flag, []() {
        qDBusRegisterMetaType<PolkitSubject>();
        qDBusRegisterMetaType<PolkitResult>();
        qDBusRegisterMetaType<QMap<QString, QString>>();
    });

    auto bus = QDBusConnection::systemBus();

    auto msg = QDBusMessage::createMethodCall(QString::fromLatin1(POLKIT_SERVICE),
                                              QString::fromLatin1(POLKIT_PATH),
                                              QString::fromLatin1(POLKIT_INTERFACE),
                                              QStringLiteral("CheckAuthorization"));

    PolkitSubject subject;
    subject.kind = QStringLiteral("system-bus-name");
    subject.details.insert(QStringLiteral("name"),
                           QVariant::fromValue(QString::fromStdString(systemBusName)));

    // 4. (sa{sv}), s, a{ss}, u, s
    msg << QVariant::fromValue(subject) << QString::fromStdString(actionId)
        << QVariant::fromValue(QMap<QString, QString>())
        << static_cast<uint>(userInteraction ? 1 : 0) << QString();

    QDBusMessage reply = bus.call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return LINGLONG_ERR(
          fmt::format("polkit check failed: {}", reply.errorMessage().toStdString()));
    }

    auto arguments = reply.arguments();
    if (arguments.isEmpty()) {
        return LINGLONG_ERR("invalid polkit response: empty arguments");
    }

    PolkitResult res;
    arguments.at(0).value<QDBusArgument>() >> res;
    return res.isAuthorized;
}

} // namespace linglong::service
