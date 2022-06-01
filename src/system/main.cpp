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
#include "module/util/log_handler.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");
    // qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} [%{appname}] [%{type}] %{message}");
    // 安装消息处理函数
    LOG_HANDLER->installMessageHandler();

    linglong::service::registerAllMetaType();

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.registerService("com.deepin.linglong.SystemPackageManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    SystemPackageManagerAdaptor pma(SYSTEM_MANAGER_HELPER);

    dbus.registerObject("/com/deepin/linglong/SystemPackageManager", SYSTEM_MANAGER_HELPER);

    return QCoreApplication::exec();
}
