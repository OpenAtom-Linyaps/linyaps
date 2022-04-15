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
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.PackageManager")    /**< D-Bus Interface */

    friend class linglong::util::Singleton<PackageManager>;   /**< friend class */

public Q_SLOTS:
    QString Status();   /**< get the status of package manager */

    /**
     * @brief download the package
     * 
     * @param packageIdList 玲珑包ID列表
     * @param savePath 下载存储路径
     * @return RetMessageList 消息列表 
     *          state:true or false 
     *          code:状态码(0:成功,1:失败)
     *          message:消息
     *
     */
    RetMessageList Download(const QStringList &packageIdList, const QString savePath);
    RetMessageList Install(const QStringList &packageIdList, const ParamStringMap &paramMap = {});
    RetMessageList Uninstall(const QStringList &packageIdList, const ParamStringMap &paramMap = {});
    RetMessageList Update(const QStringList &packageIdList, const ParamStringMap &paramMap = {});
    QString UpdateAll();

    AppMetaInfoList Query(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

    QString Import(const QStringList &packagePathList);

    RetMessageList Start(const QString &packageId, const ParamStringMap &paramMap = {});
    RetMessageList Stop(const QString &containerId);
    ContainerList ListContainer();

private:
    QScopedPointer<PackageManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

protected:
    PackageManager();
    ~PackageManager() override;
};