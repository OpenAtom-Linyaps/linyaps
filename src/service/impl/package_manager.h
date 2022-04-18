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
#include "package_manager_impl.h"
#include "qdbus_retmsg.h"

#include "package_manager_flatpak_impl.h"
#include "package_manager_option.h"

class PackageManagerPrivate;    /**< forward declaration PackageManagerPrivate */

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
     * @brief get status of package manager
     * @return
     */
    QString Status();

    /**
     * @brief download the package
     * 
     * @param packageIdList 玲珑包ID列表
     * @param savePath 下载存储路径
     * @return RetMessageList 消息列表 \n
     *          state:true or false \n
     *          code:状态码(0:成功,1:失败) \n
     *          message:消息
     *
     */
    RetMessageList Download(const QStringList &packageIdList, const QString savePath);

    /**
     * @brief 
     * 
     * @param packageIdList 
     * @param paramMap 
     * @return RetMessageList 
     */
    RetMessageList Install(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

    /**
     * @brief 
     * 
     * @param packageIdList 
     * @param paramMap 
     * @return RetMessageList 
     */
    RetMessageList Uninstall(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

    /**
     * @brief 
     * 
     * @param packageIdList 
     * @param paramMap 
     * @return RetMessageList 
     */
    RetMessageList Update(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

    /**
     * @brief 
     * 
     * @return QString 
     */
    QString UpdateAll();

    /**
     * @brief 
     * 
     * @param packageIdList 
     * @param paramMap 
     * @return AppMetaInfoList 
     */
    AppMetaInfoList Query(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

    /**
     * @brief 
     * 
     * @param packageIdList 
     * @return QString 
     */
    QString Import(const QStringList &packagePathList);

    /**
     * @brief 
     * 
     * @param packageId 
     * @param paramMap 
     * @return RetMessageList 
     */
    RetMessageList Start(const QString &packageId, const ParamStringMap &paramMap = {});

    /**
     * @brief 
     * 
     * @param containerId 
     * @return RetMessageList 
     */
    RetMessageList Stop(const QString &containerId);

    /**
     * @brief 
     * 
     * @return ContainerList 
     */
    ContainerList ListContainer();

private:
    QScopedPointer<PackageManagerPrivate> dd_ptr;   ///< private data
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

protected:
    PackageManager();
    ~PackageManager() override;
};