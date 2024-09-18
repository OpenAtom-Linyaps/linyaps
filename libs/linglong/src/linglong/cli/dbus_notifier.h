// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

// All class which in this file is used to interact with user by notification
// https://specifications.freedesktop.org/notification-spec/latest/

#include "interactive_notifier.h"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/utils/error/error.h"

#include <QDBusInterface>
#include <QObject>

namespace linglong::cli {

class DBusNotifier final : public QObject, public InteractiveNotifier
{
    Q_OBJECT
public:
    DBusNotifier();
    utils::error::Result<api::types::v1::InteractionReply>
    request(const api::types::v1::InteractionRequest &request) override;
    utils::error::Result<void> notify(const api::types::v1::InteractionRequest &request) override;

Q_SIGNALS:
    void actionInvoked(quint32 ID, QString action, QPrivateSignal);
    void notificationClosed(quint32 ID, quint32 reason, QPrivateSignal);

private:
    enum class CloseReason {
        Expired = 1, // The notification expired.
        Dismissed,   // The notification was dismissed by the user.
        CloseByCall, // The notification was closed by a call to CloseNotification.
        Undefined,   // Undefined/reserved reasons.
        NoReason     // custom defined for initializing variable
    };

    [[nodiscard]] linglong::utils::error::Result<QStringList> getCapabilities() noexcept;
    linglong::utils::error::Result<quint32> notify(const QString &appName,
                                                   quint32 replaceID,
                                                   const QString &icon,
                                                   const QString &summary,
                                                   const QString &body,
                                                   const QStringList &actions,
                                                   const QVariantMap &hints,
                                                   qint32 expireTimeout) noexcept;
    QDBusInterface inter;

private Q_SLOTS:
    void forwardActionInvoked(quint32 ID, QString action);
    void forwardNotificationClosed(quint32 ID, quint32 reason);
};

} // namespace linglong::cli
