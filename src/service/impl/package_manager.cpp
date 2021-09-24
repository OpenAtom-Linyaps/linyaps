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
#include <module/util/fs.h>

#include "job_manager.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;

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

/*!
 * 执行软件包
 * @param packageID 软件包的appid
 */
QString PackageManager::Start(const QString &packageID)
{
    Q_D(PackageManager);

    qDebug() << "start package" << packageID;
    return JobManager::instance()->CreateJob([=](Job *jr) {
        QString config = nullptr;
        if (fileExists("~/.linglong/" + packageID + ".yaml")) {
            config = "~/.linglong/" + packageID + ".yaml";
        } else if (fileExists("/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml")) {
            config = "/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml";
        } else {
            auto config_dir = listDirFolders("/deepin/linglong/layers/" + packageID);
            if (config_dir.isEmpty()) {
                qInfo() << "loader:: can not found config file!";
                return;
            }
            if (config_dir.size() >= 2) {
                std::sort(config_dir.begin(), config_dir.end(),
                          [](const QString &s1, const QString &s2) {
                              auto v1 = s1.split("/").last();
                              auto v2 = s2.split("/").last();
                              return v1.toDouble() > v2.toDouble();
                          });
            }
            config = config_dir.first() + "/" + packageID + ".yaml";
        }

        qDebug() << "load package" << packageID << " config " << config;

        auto app = App::load(config);
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