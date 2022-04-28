/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QCoreApplication>

#include "module/runtime/app.h"
#include "impl/register_meta_type.h"
#include "packagemanageradaptor.h"
#include "jobmanageradaptor.h"
#include "module/runtime/runtime.h"
#include "module/util/log_handler.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    // 安装消息处理函数
    LOG_HANDLER->installMessageHandler();

    linglong::runtime::registerAllMetaType();
    linglong::package::registerAllMetaType();
    linglong::service::registerAllMetaType();

    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerService("com.deepin.linglong.AppManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    PackageManagerAdaptor pma(PACKAGE_MANAGER);
    JobManagerAdaptor jma(JobManager::instance());

    // TODO(se): 需要进行错误处理
    dbus.registerObject("/com/deepin/linglong/PackageManager", PACKAGE_MANAGER);
    dbus.registerObject("/com/deepin/linglong/JobManager", JobManager::instance());

    return QCoreApplication::exec();
}
