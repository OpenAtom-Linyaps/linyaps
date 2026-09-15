// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <QDBusInterface>
#include <QObject>

#include <string>
#include <utility>
#include <vector>

namespace linglong::ctk::detect {

class DBusNotifier final : public QObject
{
    Q_OBJECT

public:
    struct NotificationResult
    {
        std::string action;
        bool userInteracted = false;

        NotificationResult() = default;

        NotificationResult(std::string action, bool userInteracted)
            : action(std::move(action))
            , userInteracted(userInteracted)
        {
        }
    };

    struct NotificationRequest
    {
        std::string summary;
        std::string body;
        std::vector<std::string> actions;
        std::string appName;
        std::string icon;
        int timeout = 0;
    };

    explicit DBusNotifier(QObject *parent = nullptr);

    utils::error::Result<void> init();
    utils::error::Result<NotificationResult>
    sendInteractiveNotification(const NotificationRequest &request);
    utils::error::Result<void> sendSimpleNotification(const NotificationRequest &request);

Q_SIGNALS:
    void actionInvoked(quint32 id, QString action, QPrivateSignal);
    void notificationClosed(quint32 id, quint32 reason, QPrivateSignal);

private Q_SLOTS:
    void forwardActionInvoked(quint32 id, QString action);
    void forwardNotificationClosed(quint32 id, quint32 reason);

private:
    QDBusInterface dbusInterface_;
};

} // namespace linglong::ctk::detect
