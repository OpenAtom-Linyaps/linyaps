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
#include <DSingleton>

#include "json_register_inc.h"
#include "module/package/package.h"
#include "module/runtime/container.h"
#include "module/package/pkginfo.h"
#include "module/util/singleton.h"
#include "package_manager_impl.h"
#include "qdbus_retmsg.h"

using linglong::service::util::AppInstance;

class PackageManagerPrivate;
class PackageManager : public QObject
    , protected QDBusContext
    , public Dtk::Core::DSingleton<PackageManager>
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.linglong.PackageManager")

    friend class Dtk::Core::DSingleton<PackageManager>;

public Q_SLOTS:
    QString Status();

    RetMessageList Download(const QStringList &packageIDList, const QString savePath);
    RetMessageList Install(const QStringList &packageIDList);
    RetMessageList Uninstall(const QStringList &packageIDList);
    QString Update(const QStringList &packageIDList);
    QString UpdateAll();

    PKGInfoList Query(const QStringList &packageIDList);

    QString Import(const QStringList &packagePathList);

    QString Start(const QString &packageID);
    void Stop(const QString &containerID);
    ContainerList ListContainer();

    /*!
     * QDbusRetInfo
     * @param packageIDList appid
     * @return PKGInfoList
     */
    PKGInfoList QDbusRetInfo(const QStringList &packageIDList)
    {
        PKGInfoList list;
        qInfo() << "recv: " + QString::number(packageIDList.size());
        for (const auto &it : packageIDList) {
            qInfo() << "appid:" << it;
        }
        for (int i = 0; i < 3; i++) {
            auto c = QPointer<PKGInfo>(new PKGInfo);
            c->appid = "org.deepin.test-" + QString::number(i);
            c->appname = "test-" + QString::number(i);
            c->version = "v" + QString::number(i);
            list.push_back(c);
        }
        return list;
    };

    /*!
     * QDbusMessageRet
     * @return RetMessageList
     */
    RetMessageList QDbusMessageRet(void)
    {
        RetMessageList list;

        qInfo() << "call: QDbusMessageRet";

        for (int i = 0; i < 3; i++) {
            auto c = QPointer<RetMessage>(new RetMessage);
            c->state = false;
            c->code = 404;
            c->message = "not found!";
            list.push_back(c);
        }

        return list;
    };

protected:
    AppInstance *app_instance_list ;

protected:
    PackageManager();
    ~PackageManager() override;

private:
    QScopedPointer<PackageManagerPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), PackageManager)

    AppStreamPkgInfo appStreamPkgInfo;
};