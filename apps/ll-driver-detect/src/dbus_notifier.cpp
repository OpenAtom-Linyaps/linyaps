// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbus_notifier.h"

#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <QDBusReply>
#include <QEventLoop>

namespace linglong::driver::detect {

DBusNotifier::DBusNotifier(QObject *parent)
    : QObject(parent)
    , dbusInterface_("org.freedesktop.Notifications",
                     "/org/freedesktop/Notifications",
                     "org.freedesktop.Notifications")
{
}

utils::error::Result<void> DBusNotifier::init()
{
    LINGLONG_TRACE("init dbus notifier")

    auto connection = dbusInterface_.connection();

    if (!connection.connect(dbusInterface_.service(),
                            dbusInterface_.path(),
                            dbusInterface_.interface(),
                            "ActionInvoked",
                            this,
                            SLOT(forwardActionInvoked(quint32, QString)))) {
        return LINGLONG_ERR("couldn't connect to signal ActionInvoked:"
                            + connection.lastError().message());
    }

    if (!connection.connect(dbusInterface_.service(),
                            dbusInterface_.path(),
                            dbusInterface_.interface(),
                            "NotificationClosed",
                            this,
                            SLOT(forwardNotificationClosed(quint32, quint32)))) {
        return LINGLONG_ERR("couldn't connect to signal NotificationClosed:"
                            + connection.lastError().message());
    }
    return LINGLONG_OK;
}

utils::error::Result<DBusNotifier::NotificationResult>
DBusNotifier::sendInteractiveNotification(const NotificationRequest &request)
{
    LINGLONG_TRACE("send request through dbus notification")

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

    auto reply = sendDBusNotification(request.appName,
                                      0,            // replaceId
                                      request.icon, // icon
                                      request.summary,
                                      request.body,
                                      request.actions,
                                      QVariantMap(), // hints
                                      0);

    if (!reply) {
        return LINGLONG_ERR(reply.error().message());
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
    case CloseReason::Expired:
    case CloseReason::Dismissed:
    case CloseReason::CloseByCall:
        return NotificationResult(choice, true);
    case CloseReason::Undefined:
        return LINGLONG_ERR("server return an undefined reason");
    case CloseReason::NoReason:
        break;
    }

    // This part of the code should not be reachable.
    // To be safe, return an error instead of aborting.
    return LINGLONG_ERR("Unhandled notification close reason");
}

utils::error::Result<void> DBusNotifier::sendSimpleNotification(const NotificationRequest &request)
{
    LINGLONG_TRACE("sendSimpleNotification");

    // 发送通知（无动作，简单通知）
    auto notificationId = sendDBusNotification(request.appName,
                                               0,            // replaceId
                                               request.icon, // icon
                                               request.summary,
                                               request.body,
                                               QStringList(), // no actions
                                               QVariantMap(), // hints
                                               request.timeout);

    if (!notificationId) {
        return LINGLONG_ERR(notificationId.error().message());
    }

    LogD("Simple notification sent with ID: {}", *notificationId);
    return LINGLONG_OK;
}

utils::error::Result<quint32> DBusNotifier::sendDBusNotification(const QString &appName,
                                                                 quint32 replaceId,
                                                                 const QString &icon,
                                                                 const QString &summary,
                                                                 const QString &body,
                                                                 const QStringList &actions,
                                                                 const QVariantMap &hints,
                                                                 qint32 timeout)
{
    LINGLONG_TRACE("sendDBusNotification");

    if (!dbusInterface_.isValid()) {
        return LINGLONG_ERR("DBus interface is not valid");
    }

    // 调用Notify方法
    QDBusMessage message =
      dbusInterface_
        .call("Notify", appName, replaceId, icon, summary, body, actions, hints, timeout);

    if (message.type() == QDBusMessage::ErrorMessage) {
        return LINGLONG_ERR(QString("Failed to send notification: %1").arg(message.errorMessage()));
    }

    if (message.arguments().isEmpty()) {
        return LINGLONG_ERR("No notification ID returned");
    }

    quint32 notificationId = message.arguments().first().toUInt();
    LogD("Notification sent with ID: {}", notificationId);

    return notificationId;
}

void DBusNotifier::forwardActionInvoked(quint32 ID, QString action)
{
    Q_EMIT actionInvoked(ID, action, QPrivateSignal{});
}

void DBusNotifier::forwardNotificationClosed(quint32 ID, quint32 reason)
{
    Q_EMIT notificationClosed(ID, reason, QPrivateSignal{});
}

} // namespace linglong::driver::detect
