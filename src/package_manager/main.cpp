/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/jobmanageradaptor.h"
#include "module/packagemanageradaptor.h"
#include "module/util/log/log_handler.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");
    // 安装消息处理函数
    LOG_HANDLER->installMessageHandler();

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
