// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbus_notifier.h"

#include "linglong/utils/finally/finally.h"

#include <QDBusReply>
#include <QDebug>
#include <QEventLoop>

namespace linglong::cli {

DBusNotifier::DBusNotifier()
    : inter("org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications")
{
    auto connection = inter.connection();

    if (!connection.connect(inter.service(),
                            inter.path(),
                            inter.interface(),
                            "ActionInvoked",
                            this,
                            SLOT(forwardActionInvoked(quint32, QString)))) {
        throw std::runtime_error{ "couldn't connect to signal ActionInvoked:"
                                  + connection.lastError().message().toStdString() };
    }

    if (!connection.connect(inter.service(),
                            inter.path(),
                            inter.interface(),
                            "NotificationClosed",
                            this,
                            SLOT(forwardNotificationClosed(quint32, quint32)))) {
        throw std::runtime_error{ "couldn't connect to signal NotificationClosed" };
    }
}

linglong::utils::error::Result<QStringList> DBusNotifier::getCapabilities() noexcept
{
    LINGLONG_TRACE("get capabilities from service")

    QDBusReply<QStringList> reply = inter.call(QDBus::Block, "GetCapabilities");
    if (!reply.isValid()) {
        auto msg = "invoke GetCapabilities error:" + reply.error().message();
        return LINGLONG_ERR(msg);
    }

    return reply.value();
}

linglong::utils::error::Result<quint32> DBusNotifier::notify(const QString &appName,
                                                             quint32 replaceID,
                                                             const QString &icon,
                                                             const QString &summary,
                                                             const QString &body,
                                                             const QStringList &actions,
                                                             const QVariantMap &hints,
                                                             qint32 expireTimeout) noexcept
{
    LINGLONG_TRACE("send notification to service")

    QDBusReply<quint32> reply = inter.call(QDBus::Block,
                                           "Notify",
                                           appName,
                                           replaceID,
                                           icon,
                                           summary,
                                           body,
                                           actions,
                                           hints,
                                           expireTimeout);
    if (!reply.isValid()) {
        auto msg = "send notification error:" + reply.error().message();
        return LINGLONG_ERR(msg);
    }

    return reply.value();
}

utils::error::Result<void> DBusNotifier::notify(const api::types::v1::InteractionRequest &request)
{
    LINGLONG_TRACE("send notification through dbus notification")

    auto reply = this->notify(QString::fromStdString(request.appName),
                              0,
                              "",
                              QString::fromStdString(request.summary),
                              QString::fromStdString(request.body.value_or("")),
                              {},
                              { { "urgency", 1 } },
                              -1);
    if (!reply) {
        return LINGLONG_ERR("failed to notify: " + reply.error().message());
    }

    return LINGLONG_OK;
}

utils::error::Result<api::types::v1::InteractionReply>
DBusNotifier::request(const api::types::v1::InteractionRequest &request)
{
    LINGLONG_TRACE("send request through dbus notification")

    QStringList output;
    if (request.actions) {
        for (const auto &str : request.actions.value()) {
            output.append(QString::fromStdString(str));
        }
    }

    quint32 notificationID{ 0 };
    QString choice;
    auto reason{ CloseReason::NoReason };

    auto invokeCon =
      QObject::connect(this,
                       &DBusNotifier::actionInvoked,
                       [&notificationID, &choice](quint32 ID, const QString &action) {
                           if (notificationID != 0 && notificationID == ID) {
                               choice = action;
                           }
                       });
    auto disInvokeCon = utils::finally::finally([&invokeCon] {
        QObject::disconnect(invokeCon);
    });

    auto closedCon = QObject::connect(this,
                                      &DBusNotifier::notificationClosed,
                                      [&notificationID, &reason](quint32 ID, quint32 newReason) {
                                          if (notificationID != 0 && notificationID == ID) {
                                              reason = static_cast<CloseReason>(newReason);
                                          }
                                      });
    auto disClosedCon = utils::finally::finally([&closedCon] {
        QObject::disconnect(closedCon);
    });

    auto reply = this->notify(QString::fromStdString(request.appName),
                              0,
                              "",
                              QString::fromStdString(request.summary),
                              QString::fromStdString(request.body.value_or("")),
                              output,
                              { { "urgency", 1 } },
                              0);
    if (!reply) {
        return LINGLONG_ERR("failed to notify: " + reply.error().message());
    }
    notificationID = *reply;

    QEventLoop loop;
    std::function<void()> checker = std::function{ [&loop, &reason, &checker]() {
        if (reason != CloseReason::NoReason) {
            loop.exit();
        }
        QMetaObject::invokeMethod(&loop, checker, Qt::QueuedConnection);
    } };

    QMetaObject::invokeMethod(&loop, checker, Qt::QueuedConnection);
    loop.exec();

    switch (reason) {
    case CloseReason::Dismissed:
        [[fallthrough]];
    case CloseReason::CloseByCall:
        return api::types::v1::InteractionReply{ .action = choice.toStdString() };
    case CloseReason::Undefined:
        return LINGLONG_ERR("server return an undefined reason");
    case CloseReason::Expired:
        [[fallthrough]];
    case CloseReason::NoReason:
        break;
    }

    qCritical() << "unexpected reason:" << static_cast<int>(reason)
                << "control flow shouldn't be here:" << __FILE__ << __LINE__;
    std::abort();
}

void DBusNotifier::forwardActionInvoked(quint32 ID, QString action)
{
    Q_EMIT actionInvoked(ID, action, QPrivateSignal{});
}

void DBusNotifier::forwardNotificationClosed(quint32 ID, quint32 reason)
{
    Q_EMIT notificationClosed(ID, reason, QPrivateSignal{});
}

} // namespace linglong::cli
