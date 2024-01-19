/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_H_
#define LINGLONG_SRC_SERVICE_IMPL_APP_MANAGER_H_

#include "linglong/dbus_ipc/package_manager_param.h"
#include "linglong/dbus_ipc/param_option.h"
#include "linglong/dbus_ipc/reply.h"
#include "linglong/package/bundle.h"
#include "linglong/package/ref.h"
#include "linglong/repo/repo.h"
#include "linglong/runtime/app.h"
#include "linglong/util/error.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"

#include <QDBusArgument>
#include <QDBusContext>
#include <QFuture>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QThreadPool>
#include <QtConcurrent>

namespace linglong {
namespace service {
class AppManagerPrivate; /**< forward declaration AppManagerPrivate */

/**
 * @brief The AppManager class
 * @details AppManager is a singleton class, and it is used to manage the package
 *          running state information.
 */
class AppManager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.AppManager")

public:
    AppManager(repo::Repo &repo, ocppi::cli::CLI &ociCli);
    AppManager(const AppManager &) = delete;
    AppManager(AppManager &&) = delete;
    auto operator=(const AppManager &) -> AppManager & = delete;
    auto operator=(AppManager &&) -> AppManager & = delete;
    ~AppManager() override = default;

public Q_SLOTS:

    linglong::utils::error::Result<void> Run(const RunParamOption &paramOption);

    /**
     * @brief 运行应用
     *
     * @param paramOption 启动命令参数
     *
     * @return Reply 同Install
     */
    auto Start(const RunParamOption &paramOption) -> Reply;

    /**
     * @brief 运行命令
     *
     * @param paramOption 启动命令参数
     *
     * @return Reply 同 Install
     */
    auto Exec(const ExecParamOption &paramOption) -> Reply;

    /**
     * @brief 退出应用
     *
     * @param containerId 运行应用容器对应的Id（使用ListContainer查询）
     *
     * @return Reply 执行结果信息
     */
    auto Stop(const QString &containerId) -> Reply;

    /**
     * @brief 查询正在运行的应用信息
     *
     * @return ContainerList \n
     *         Id 容器id \n
     *         pid 容器对应的进程id \n
     *         packageName 应用名称 \n
     *         workingDirectory 应用运行目录
     */
    auto ListContainer() -> QueryReply;

public:
    // FIXME: ??? why this public?
    QScopedPointer<QThreadPool> runPool; ///< 启动应用线程池

private:
    QMap<QString, QSharedPointer<linglong::runtime::App> > apps = {};
    linglong::repo::Repo &repo;
    ocppi::cli::CLI &ociCli;
};

} // namespace service
} // namespace linglong

#define RUN_POOL_MAX_THREAD 100000000 ///< 启动应用线程池最大线程数
#endif
