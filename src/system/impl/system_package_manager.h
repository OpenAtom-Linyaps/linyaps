/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     HuQinghong <huqinghong@uniontech.com>
 *
 * Maintainer: HuQinghong <huqinghong@uniontech.com>
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


#include "module/util/package_manager_param.h"
#include "module/util/singleton.h"
#include "module/util/status_code.h"
#include "service/impl/reply.h"
#include "service/impl/param_option.h"
#include "module/package/package.h"
#include "package_manager_flatpak_impl.h"
#include "service/impl/register_meta_type.h"
#include "module/runtime/container.h"

namespace linglong {
namespace service {
class SystemPackageManagerPrivate;

/**
 * @brief The PackageManager class
 * @details PackageManager is a singleton class, and it is used to manage the package
 *          information.
 */
class SystemPackageManager
    : public QObject
    , protected QDBusContext
    , public linglong::util::Singleton<SystemPackageManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.SystemPackageManager")

    friend class linglong::util::Singleton<SystemPackageManager>;

public Q_SLOTS:

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
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply GetDownloadStatus(const ParamOption &paramOption);

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

public:
    QScopedPointer<QThreadPool> pool; ///< 下载、卸载、更新应用线程池

private:
    QScopedPointer<SystemPackageManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), SystemPackageManager)

protected:
    SystemPackageManager();
    ~SystemPackageManager() override;
};

} // namespace service
} // namespace linglong

#define POOL_MAX_THREAD 10 ///< 下载、卸载、更新应用线程池最大线程数
#define SYSTEM_MANAGER_HELPER linglong::service::SystemPackageManager::instance()

