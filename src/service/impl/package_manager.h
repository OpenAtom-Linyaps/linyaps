/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDBusArgument>
#include <QDBusContext>
#include <QList>
#include <QObject>
#include <QScopedPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>

#include "register_meta_type.h"
#include "module/package/package.h"
#include "module/runtime/container.h"
#include "module/util/package_manager_param.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"
#include "reply.h"
#include "param_option.h"

namespace linglong {
namespace service {
class PackageManagerPrivate; /**< forward declaration PackageManagerPrivate */

/**
 * @brief The PackageManager class
 * @details PackageManager is a singleton class, and it is used to manage the package
 *          information.
 */
class PackageManager
    : public QObject
    , protected QDBusContext
    , public linglong::util::Singleton<PackageManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.PackageManager")

    friend class linglong::util::Singleton<PackageManager>;

public Q_SLOTS:

    /**
     * @brief 查询包管理服务状态
     *
     * @return "active"
     */
    QString Status();

    /**
     * @brief download the package
     *
     * @param paramOption 下载参数
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply Download(const DownloadParamOption &downloadParamOption);

    /**
     * @brief 查询软件包下载安装状态
     *
     * @param paramOption 查询参数
     * @param type 查询类型 0:查询应用安装进度 1:查询应用更新进度
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply GetDownloadStatus(const ParamOption &paramOption, int type);

    /**
     * @brief 安装软件包
     *
     * @param installParamOption 安装参数
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply Install(const InstallParamOption &installParamOption);

    /**
     * @brief 卸载软件包
     *
     * @param paramOption 卸载参数
     *
     * @return Reply 同Install
     */
    Reply Uninstall(const UninstallParamOption &paramOption);

    /**
     * @brief 更新软件包
     *
     * @param paramOption 更新包参数
     *
     * @return Reply 同Install
     */
    Reply Update(const ParamOption &paramOption);

    /**
     * @brief 查询软件包信息
     *
     * @param paramOption 查询命令参数
     *
     * @return QueryReply dbus方法调用应答 \n
     *         code 状态码 \n
     *         message 状态信息 \n
     *         result 查询结果
     */
    QueryReply Query(const QueryParamOption &paramOption);

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
    QScopedPointer<PackageManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

protected:
    PackageManager();
    ~PackageManager() override;
};

} // namespace service
} // namespace linglong

#define RUN_POOL_MAX_THREAD 100000000 ///< 启动应用线程池最大线程数
#define PACKAGE_MANAGER linglong::service::PackageManager::instance()
