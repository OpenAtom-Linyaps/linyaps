/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/adaptors/job_manager/job_manager1.h"
#include "linglong/adaptors/package_manager/package_manager1.h"
#include "linglong/dbus_ipc/workaround.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.registerService("org.deepin.linglong.PackageManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    linglong::adaptors::package_manger::PackageManager1 packageManagerAdaptor(PACKAGE_MANAGER);
    linglong::adaptors::job_manger::JobManager1 jma(linglong::job_manager::JobManager::instance());

    dbus.registerObject("/org/deepin/linglong/PackageManager", PACKAGE_MANAGER);
    dbus.registerObject("/org/deepin/linglong/JobManager",
                        linglong::job_manager::JobManager::instance());

    return QCoreApplication::exec();
}
