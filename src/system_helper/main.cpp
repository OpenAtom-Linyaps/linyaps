/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QDebug>
#include <QCoreApplication>
#include <QDBusConnection>

#include "system_helper.h"

#include "systemhelperadaptor.h"

int main(int argc, char *argv[])
{
    using namespace linglong::system::helper;
    QCoreApplication app(argc, argv);

    SystemHelper systemHelper;
    SystemHelperAdaptor systemHelperAdaptor(&systemHelper);

    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.registerService(systemHelper.ServiceName)) {
        qCritical() << "registerService failed" << dbus.lastError();
        return -1;
    }

    if (!dbus.registerObject(systemHelper.ServicePath, &systemHelper)) {
        qCritical() << "registerObject failed" << dbus.lastError();
        return -1;
    }

    return app.exec();
}