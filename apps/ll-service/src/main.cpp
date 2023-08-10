/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "dbusgen/AppManagerAdaptor.h"
#include "linglong/dbus_ipc/workaround.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    registerDBusParam();

    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerService("org.deepin.linglong.AppManager")) {
        qCritical() << "service exist" << dbus.lastError();
        return -1;
    }

    AppManagerAdaptor pma(APP_MANAGER);

    // TODO(se): 需要进行错误处理
    dbus.registerObject("/org/deepin/linglong/AppManager", APP_MANAGER);

    app.connect(&app, &QCoreApplication::aboutToQuit, &app, [&] {
        dbus.unregisterObject("/org/deepin/linglong/AppManager");
        dbus.unregisterService("org.deepin.linglong.AppManager");
    });

    return QCoreApplication::exec();
}
