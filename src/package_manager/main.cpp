/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     HuQinghong <huqinghong@uniontech.com>
 *
 * Maintainer: HuQinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QCoreApplication>

#include "module/dbus_ipc/register_meta_type.h"
#include "packagemanageradaptor.h"
#include "jobmanageradaptor.h"
#include "module/util/log/log_handler.h"
#include "module/repo/repo.h"

using namespace linglong;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");
    // 安装消息处理函数
    LOG_HANDLER->installMessageHandler();

    linglong::package::registerAllMetaType();
    linglong::service::registerAllMetaType();
    linglong::repo::registerAllMetaType();

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.registerService("org.deepin.linglong.PackageManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    PackageManagerAdaptor packageManagerAdaptor(service::SystemPackageManager::instance());
    JobManagerAdaptor jma(JobManager::instance());

    dbus.registerObject("/org/deepin/linglong/PackageManager", service::SystemPackageManager::instance());
    dbus.registerObject("/com/deepin/linglong/JobManager", JobManager::instance());

    return QCoreApplication::exec();
}
