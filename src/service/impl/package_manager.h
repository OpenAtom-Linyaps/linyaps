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

#include "json_register_inc.h"
#include "module/package/package.h"
#include "module/runtime/container.h"
#include "module/util/package_manager_param.h"
#include "module/util/singleton.h"
#include "qdbus_retmsg.h"
#include "reply.h"
#include "param_option.h"
#include "package_manager_impl.h"
#include "package_manager_flatpak_impl.h"
#include "package_manager_option.h"

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
    linglong::service::Reply Download(const linglong::service::DownloadParamOption &downloadParamOption);

    /**
     * @brief 安装软件包
     *
     * @param installParamOption 安装参数
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    linglong::service::Reply Install(const linglong::service::InstallParamOption &installParamOption);

    /**
     * @brief 卸载软件包
     *
     * @param paramOption 卸载参数
     *
     * @return linglong::service::Reply 同Install
     */
    linglong::service::Reply Uninstall(const linglong::service::UninstallParamOption &paramOption);

    /**
     * @brief 更新软件包
     *
     * @param paramOption 更新包参数
     *
     * @return Reply 同Install
     */
    linglong::service::Reply Update(const linglong::service::ParamOption &paramOption);

    /**
     * @brief
     *
     * @return QString
     */
    QString UpdateAll();

    /**
     * @brief 查询软件包信息
     *
     * @param paramOption 查询命令参数
     *
     * @return AppMetaInfoList 查询结果信息 \n
     *         appId 软件包appId \n
     *         name  软件包名称 \n
     *         version 软件包版本号 \n
     *         kind  软件包类型，app和runtime \n
     *         runtime 软件包依赖的runtime对应的appId \n
     *         uabUrl 软件包对应的uab存储地址 \n
     *         repoName 软件包所属远端仓库名称 \n
     *         description 软件包描述信息 \n
     *         user 安装应用对应的用户
     */
    linglong::package::AppMetaInfoList Query(const linglong::service::QueryParamOption &paramOption);

    /**
     * @brief
     *
     * @param packageIdList
     * @return QString
     */
    QString Import(const QStringList &packagePathList);

    /**
     * @brief 运行应用
     *
     * @param packageId 应用appId
     * @param paramMap 运行命令参数 \n
     *        key为version时，value范围不涉及，用于运行指定版本应用 \n
     *        key为repo-point，value为flatpak，用于运行flatpak类型应用 \n
     *        key为no-proxy，value为""，用于以非代理模式运行应用
     *
     * @return RetMessageList 同Install
     */
    RetMessageList Start(const QString &packageId, const ParamStringMap &paramMap = {});

    /**
     * @brief 退出应用
     *
     * @param containerId 运行应用容器对应的Id（使用ListContainer查询）
     *
     * @return RetMessageList 同Install
     */
    RetMessageList Stop(const QString &containerId);

    /**
     * @brief 查询正在运行的应用信息
     *
     * @return ContainerList \n
     *         Id 容器id \n
     *         pid 容器对应的进程id \n
     *         packageName 应用名称 \n
     *         workingDirectory 应用运行目录
     */
    ContainerList ListContainer();

private:
    QScopedPointer<PackageManagerPrivate> dd_ptr; ///< private data
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

protected:
    PackageManager();
    ~PackageManager() override;
};
