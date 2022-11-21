/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_H_
#define LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_H_

#include <QDBusArgument>
#include <QDBusContext>
#include <QFuture>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

#include "module/dbus_ipc/package_manager_param.h"
#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/register_meta_type.h"
#include "module/dbus_ipc/reply.h"
#include "module/package/package.h"
#include "module/runtime/container.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"

namespace linglong {
namespace service {
class AppManagerPrivate; /**< forward declaration AppManagerPrivate */

/**
 * @brief The AppManager class
 * @details AppManager is a singleton class, and it is used to manage the package
 *          running state information.
 */
class AppManager
    : public QObject
    , protected QDBusContext
    , public linglong::util::Singleton<AppManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.AppManager")

    friend class linglong::util::Singleton<AppManager>;

public Q_SLOTS:

    /**
     * @brief 查询包管理服务状态
     *
     * @return "active"
     */
    QString Status();

    /**
     * @brief 运行应用
     *
     * @param paramOption 启动命令参数
     *
     * @return Reply 同Install
     */
    Reply Start(const RunParamOption &paramOption);

    /**
     * @brief 运行命令
     *
     * @param paramOption 启动命令参数
     *
     * @return Reply 同 Install
     */
    Reply Exec(const ExecParamOption &paramOption);

    /**
     * @brief 退出应用
     *
     * @param containerId 运行应用容器对应的Id（使用ListContainer查询）
     *
     * @return Reply 执行结果信息
     */
    Reply Stop(const QString &containerId);

    /**
     * @brief 查询正在运行的应用信息
     *
     * @return ContainerList \n
     *         Id 容器id \n
     *         pid 容器对应的进程id \n
     *         packageName 应用名称 \n
     *         workingDirectory 应用运行目录
     */
    QueryReply ListContainer();

    /**
     * @brief 执行终端命令
     *
     * @param exe 命令
     * @param args 参数
     *
     * @return Reply 执行结果信息
     */
    Reply RunCommand(const QString &exe, const QStringList args);

public:
    QScopedPointer<QThreadPool> runPool; ///< 启动应用线程池

private:
    QScopedPointer<AppManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), AppManager)

protected:
    AppManager();
    ~AppManager() override;
};

} // namespace service
} // namespace linglong

#define RUN_POOL_MAX_THREAD 100000000 ///< 启动应用线程池最大线程数
#define APP_MANAGER linglong::service::AppManager::instance()
#endif
