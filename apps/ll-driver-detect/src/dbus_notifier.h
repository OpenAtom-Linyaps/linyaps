// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <QDBusInterface>
#include <QObject>

namespace linglong::driver::detect {

/**
 * @brief DBus通知类
 *
 * 该类提供DBus通知功能
 */
class DBusNotifier final : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 通知结果结构
     */
    struct NotificationResult
    {
        QString action; // 用户选择的动作
        bool success;   // 通知是否成功发送

        NotificationResult()
            : success(false)
        {
        }

        NotificationResult(const QString &act, bool succ)
            : action(act)
            , success(succ)
        {
        }
    };

    /**
     * @brief 通知请求结构
     */
    struct NotificationRequest
    {
        QString summary;     // 通知标题
        QString body;        // 通知内容
        QStringList actions; // 可用动作按钮文本（成对出现：id, 文本）
        QString appName;     // 应用名称
        QString icon;        // 通知图标
        quint32 timeout;     // 超时时间（毫秒）
    };

    /**
     * @brief 构造函数
     */
    explicit DBusNotifier(QObject *parent = nullptr);

    /**
     * @brief Initializes the DBus connection and signals
     * @return A result object, holding an error if initialization fails
     */
    utils::error::Result<void> init();

    /**
     * @brief 发送交互式通知并等待用户响应
     * @param request 通知请求
     * @return 通知结果
     */
    utils::error::Result<NotificationResult>
    sendInteractiveNotification(const NotificationRequest &request);

    /**
     * @brief 发送简单通知
     * @param request 通知请求
     * @return 成功或错误信息
     */
    utils::error::Result<void> sendSimpleNotification(const NotificationRequest &request);

Q_SIGNALS:
    void actionInvoked(quint32 ID, QString action, QPrivateSignal);
    void notificationClosed(quint32 ID, quint32 reason, QPrivateSignal);

private Q_SLOTS:
    void forwardActionInvoked(quint32 ID, QString action);
    void forwardNotificationClosed(quint32 ID, quint32 reason);

private:
    enum class CloseReason {
        Expired = 1, // The notification expired.
        Dismissed,   // The notification was dismissed by the user.
        CloseByCall, // The notification was closed by a call to CloseNotification.
        Undefined,   // Undefined/reserved reasons.
        NoReason     // custom defined for initializing variable
    };

    /**
     * @brief 发送通知到DBus服务
     * @param appName 应用名称
     * @param replaceId 替换ID
     * @param icon 图标
     * @param summary 标题
     * @param body 内容
     * @param actions 动作列表
     * @param hints 提示信息
     * @param timeout 超时时间
     * @return 通知ID
     */
    utils::error::Result<quint32> sendDBusNotification(const QString &appName,
                                                       quint32 replaceId,
                                                       const QString &icon,
                                                       const QString &summary,
                                                       const QString &body,
                                                       const QStringList &actions,
                                                       const QVariantMap &hints,
                                                       qint32 timeout);

    QDBusInterface dbusInterface_; ///< DBus接口
};

} // namespace linglong::driver::detect
