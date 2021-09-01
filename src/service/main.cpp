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

#include <DLog>
#include <QCoreApplication>

#include "packagemanageradaptor.h"
#include "jobmanageradaptor.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    app.setOrganizationName("deepin");

    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    //    qRegisterMetaType<Container>("Container");
    //    qDBusRegisterMetaType<Container>();
    //    qRegisterMetaType<ContainerList>("ContainerList");
    //    qDBusRegisterMetaType<ContainerList>();

    PackageManagerAdaptor pma(PackageManager::instance());

    JobManagerAdaptor jma(JobManager::instance());

    // TODO: 需要进行错误处理
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerService("com.deepin.linglong.AppManager");
    dbus.registerObject("/com/deepin/linglong/PackageManager",
                        PackageManager::instance());
    dbus.registerObject("/com/deepin/linglong/JobManager",
                        JobManager::instance());

    return QCoreApplication::exec();
}