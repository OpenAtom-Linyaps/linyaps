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

#include "job_manager.h"

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
    {
    }

    PackageManager *q_ptr = nullptr;
};

PackageManager::PackageManager(void) = default;

PackageManager::~PackageManager() = default;

/*!
 * 在线安装软件包
 * @param packageIDList
 */
QString PackageManager::Install(const QStringList &packageIDList)
{
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
}

QString PackageManager::Update(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

PackageList PackageManager::Query(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

/*!
 * 安装本地软件包
 * @param packagePathList
 */
QString PackageManager::Import(const QStringList &packagePathList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

QString PackageManager::Start(const QString &packageID)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return "";
}

void PackageManager::Stop(const QString &containerID)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

ContainerList PackageManager::ListContainer()
{
    ContainerList list;

    setDelayedReply(true);
    sendErrorReply(QDBusError::NotSupported, message().member());

    list.push_back(Container {.ID = "container id"});
    return list;
}