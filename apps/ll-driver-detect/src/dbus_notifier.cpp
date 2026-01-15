// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbus_notifier.h"

#include "linglong/utils/log/log.h"

#include <QDBusReply>
#include <QEventLoop>
#include <QTimer>

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
    bool userInteracted = false;
    QEventLoop loop;

    struct SignalGuard
    {
        QMetaObject::Connection closeConn;
        QMetaObject::Connection actionConn;

        ~SignalGuard()
        {
            QObject::disconnect(closeConn);
            QObject::disconnect(actionConn);
        }
    } signalGuard{ {}, {} };

    // 连接信号处理函数
    signalGuard.closeConn =
      QObject::connect(this,
                       &DBusNotifier::notificationClosed,
                       [&](quint32 id, quint32 closeReason) {
                           if (notificationID != 0 && id == notificationID) {
                               LogD("Notification {} closed with reason: {}", id, closeReason);
                               loop.quit();
                           }
                       });

    signalGuard.actionConn = QObject::connect(
      this,
      &DBusNotifier::actionInvoked,
      [&](quint32 id, const QString &actionKey) {
          if (notificationID != 0 && id == notificationID) {
              LogD("Notification {} action invoked: {}", id, actionKey.toStdString());
              choice = actionKey;
              userInteracted = true;
          }
      });

    // 检查信号连接是否成功
    if (!signalGuard.closeConn || !signalGuard.actionConn) {
        return LINGLONG_ERR("Failed to connect notification signals");
    }

    // 发送通知
    auto reply = sendDBusNotification(request.appName,
                                      0,            // replaceId
                                      request.icon, // icon
                                      request.summary,
                                      request.body,
                                      request.actions,
                                      QVariantMap(), // hints
                                      request.timeout);

    if (!reply) {
        return LINGLONG_ERR(reply.error().message());
    }
    notificationID = *reply;
    LogD("Interactive notification sent with ID: {}", notificationID);

    // 设置事件循环超时处理
    QTimer::singleShot(request.timeout, &loop, &QEventLoop::quit);

    // 运行事件循环等待用户交互
    loop.exec();

    // 根据用户是否交互返回相应的结果
    if (userInteracted) {
        LogD("User selected action: {}", choice.toStdString());
        return NotificationResult(choice, true);
    } else {
        LogD("Notification closed without user interaction");
        return NotificationResult(choice, false);
    }
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
