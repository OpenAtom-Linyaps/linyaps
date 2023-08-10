/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "dbusgen/JobManagerAdaptor.h"
#include "dbusgen/PackageManagerAdaptor.h"
#include "linglong/dbus_ipc/workaround.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    registerDBusParam();

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.registerService("org.deepin.linglong.PackageManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    PackageManagerAdaptor packageManagerAdaptor(PACKAGE_MANAGER);
    JobManagerAdaptor jma(JobManager::instance());

    dbus.registerObject("/org/deepin/linglong/PackageManager", PACKAGE_MANAGER);
    dbus.registerObject("/org/deepin/linglong/JobManager", JobManager::instance());

    return QCoreApplication::exec();
}
