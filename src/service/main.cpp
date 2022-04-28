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
#include "impl/json_register_inc.h"
#include "packagemanageradaptor.h"
#include "jobmanageradaptor.h"
#include "module/runtime/runtime.h"
#include "module/util/log_handler.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    // 安装消息处理函数
    linglong::util::LogHandler::instance()->installMessageHandler();

    linglong::runtime::registerAllMetaType();
    linglong::package::registerAllMetaType();
    linglong::service::registerAllMetaType();

    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerService("com.deepin.linglong.AppManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    PackageManagerAdaptor pma(linglong::service::PackageManager::instance());
    JobManagerAdaptor jma(JobManager::instance());

    // TODO(se): 需要进行错误处理
    dbus.registerObject("/com/deepin/linglong/PackageManager", linglong::service::PackageManager::instance());
    dbus.registerObject("/com/deepin/linglong/JobManager", JobManager::instance());

    return QCoreApplication::exec();
}
