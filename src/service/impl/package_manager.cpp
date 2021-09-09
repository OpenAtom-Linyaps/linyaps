/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "package_manager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <module/runtime/app.h>

#include "job_manager.h"

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
    {
    }

    QMap<QString, QPointer<App>> apps;

    PackageManager *q_ptr = nullptr;
};

PackageManager::PackageManager()
    : dd_ptr(new PackageManagerPrivate(this))
{
}

PackageManager::~PackageManager() = default;

/*!
 * 在线安装软件包
 * @param packageIDList
 */
QString PackageManager::Install(const QStringList &packageIDList)
{
    Q_D(PackageManager);

    return JobManager::instance()->CreateJob([](Job *jr) {
        // 在这里写入真正的实现
        QProcess p;
        p.setProgram("curl");
        p.setArguments({"https://www.baidu.com"});
        p.start();
        p.waitForStarted();
        p.waitForFinished(-1);
        qDebug() << p.readAllStandardOutput();
        qDebug() << "finish" << p.exitStatus() << p.state();
    });
}

QString PackageManager::Uninstall(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::Update(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

PackageList PackageManager::Query(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*!
 * 安装本地软件包
 * @param packagePathList
 */
QString PackageManager::Import(const QStringList &packagePathList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::Start(const QString &packageID)
{
    Q_D(PackageManager);

    qDebug() << "start package" << packageID;
    return JobManager::instance()->CreateJob([=](Job *jr) {
        auto app = App::load("/tmp/test.yaml");
        if (nullptr == app) {
            qCritical() << "nullptr" << app;
            return;
        }
        d->apps[app->container->ID] = QPointer<App>(app);
        app->start();
    });
    //    sendErrorReply(QDBusError::NotSupported, message().member());
}

void PackageManager::Stop(const QString &containerID)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

ContainerList PackageManager::ListContainer()
{
    Q_D(PackageManager);
    ContainerList list;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->ID = app->container->ID;
        c->PID = app->container->PID;
        list.push_back(c);
    }
    return list;
}