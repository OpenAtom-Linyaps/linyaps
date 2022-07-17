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

#include "service/impl/register_meta_type.h"
#include "systempackagemanageradaptor.h"
#include "jobmanageradaptor.h"
#include "module/util/log_handler.h"
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
    if (!dbus.registerService("com.deepin.linglong.SystemPackageManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    SystemPackageManagerAdaptor packageManagerAdaptor(service::SystemPackageManager::instance());
    JobManagerAdaptor jma(JobManager::instance());

    dbus.registerObject("/com/deepin/linglong/SystemPackageManager", service::SystemPackageManager::instance());
    dbus.registerObject("/com/deepin/linglong/JobManager", JobManager::instance());
    return QCoreApplication::exec();
}
