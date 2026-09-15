// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbus_notifier.h"

#include "linglong/utils/finally/finally.h"
#include "linglong/utils/log/log.h"

#include <QDBusReply>
#include <QEventLoop>
#include <QTimer>

namespace linglong::ctk::detect {

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
                            + connection.lastError().message().toStdString());
    }

    if (!connection.connect(dbusInterface_.service(),
                            dbusInterface_.path(),
                            dbusInterface_.interface(),
                            "NotificationClosed",
                            this,
                            SLOT(forwardNotificationClosed(quint32, quint32)))) {
        return LINGLONG_ERR("couldn't connect to signal NotificationClosed:"
                            + connection.lastError().message().toStdString());
    }
    return LINGLONG_OK;
}

utils::error::Result<DBusNotifier::NotificationResult>
DBusNotifier::sendInteractiveNotification(const NotificationRequest &request)
{
    LINGLONG_TRACE("send interactive notification")

    quint32 notificationID = 0;
    QString choice;
    bool userInteracted = false;
    QEventLoop loop;

    QMetaObject::Connection closeConn;
    QMetaObject::Connection actionConn;

    auto disconnectSignals = utils::finally::finally([&] {
        QObject::disconnect(closeConn);
        QObject::disconnect(actionConn);
    });

    closeConn =
      QObject::connect(this, &DBusNotifier::notificationClosed, [&](quint32 id, quint32 reason) {
          if (notificationID != 0 && id == notificationID) {
              LogD("notification {} closed with reason {}", id, reason);
              loop.quit();
          }
      });

    actionConn = QObject::connect(
      this,
      &DBusNotifier::actionInvoked,
      [&](quint32 id, const QString &actionKey) {
          if (notificationID != 0 && id == notificationID) {
              LogD("notification {} action invoked: {}", id, actionKey.toStdString());
              choice = actionKey;
              userInteracted = true;
          }
      });

    if (!closeConn || !actionConn) {
        return LINGLONG_ERR("failed to connect notification signals");
    }

    QStringList actions;
    for (const auto &action : request.actions) {
        actions.append(QString::fromStdString(action));
    }

    QDBusMessage reply = dbusInterface_.call("Notify",
                                             QString::fromStdString(request.appName),
                                             static_cast<quint32>(0),
                                             QString::fromStdString(request.icon),
                                             QString::fromStdString(request.summary),
                                             QString::fromStdString(request.body),
                                             actions,
                                             QVariantMap(),
                                             request.timeout);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return LINGLONG_ERR("failed to send notification: " + reply.errorMessage().toStdString());
    }
    if (reply.arguments().isEmpty()) {
        return LINGLONG_ERR("no notification id returned");
    }
    notificationID = reply.arguments().first().toUInt();

    QTimer::singleShot(request.timeout, &loop, &QEventLoop::quit);
    loop.exec();

    if (userInteracted) {
        return NotificationResult(choice.toStdString(), true);
    }
    return NotificationResult(choice.toStdString(), false);
}

utils::error::Result<void> DBusNotifier::sendSimpleNotification(const NotificationRequest &request)
{
    LINGLONG_TRACE("send simple notification")

    QDBusMessage reply = dbusInterface_.call("Notify",
                                             QString::fromStdString(request.appName),
                                             static_cast<quint32>(0),
                                             QString::fromStdString(request.icon),
                                             QString::fromStdString(request.summary),
                                             QString::fromStdString(request.body),
                                             QStringList(),
                                             QVariantMap(),
                                             request.timeout);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return LINGLONG_ERR("failed to send notification: " + reply.errorMessage().toStdString());
    }
    return LINGLONG_OK;
}

void DBusNotifier::forwardActionInvoked(quint32 id, QString action)
{
    Q_EMIT actionInvoked(id, std::move(action), QPrivateSignal{});
}

void DBusNotifier::forwardNotificationClosed(quint32 id, quint32 reason)
{
    Q_EMIT notificationClosed(id, reason, QPrivateSignal{});
}

} // namespace linglong::ctk::detect
