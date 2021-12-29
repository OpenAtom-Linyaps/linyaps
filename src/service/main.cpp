/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <DLog>
#include <QCoreApplication>

#include "module/runtime/app.h"
#include "impl/json_register_inc.h"
#include "impl/qdbus_retmsg.h"
#include "packagemanageradaptor.h"
#include "jobmanageradaptor.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    ociJsonRegister();
    qJsonRegister<Layer>();
    qJsonRegister<Permission>();
    qJsonRegister<App>();
    

    // register qdbus type
    RegisterDbusType();

    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerService("com.deepin.linglong.AppManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    PackageManagerAdaptor pma(PackageManager::instance());
    JobManagerAdaptor jma(JobManager::instance());

    // TODO(se): 需要进行错误处理
    dbus.registerObject("/com/deepin/linglong/PackageManager",
                        PackageManager::instance());
    dbus.registerObject("/com/deepin/linglong/JobManager",
                        JobManager::instance());

    return QCoreApplication::exec();
}
