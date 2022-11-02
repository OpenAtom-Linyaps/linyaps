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

#include "module/util/log/log_handler.h"
#include "system_helper.h"
#include "privilege/privilege_install_portal.h"

#include "module/dbus_ipc/dbus_common.h"
#include "module/dbus_ipc/dbus_system_helper_common.h"
#include "systemhelperadaptor.h"

int main(int argc, char *argv[])
{
    using namespace linglong;
    using namespace linglong::system::helper;
    QCoreApplication app(argc, argv);

    LOG_HANDLER->installMessageHandler();

    qJsonRegister<linglong::system::helper::FilePortalRule>();
    qJsonRegister<linglong::system::helper::PortalRule>();

    SystemHelper systemHelper;
    SystemHelperAdaptor systemHelperAdaptor(&systemHelper);

    QCommandLineParser parser;
    QCommandLineOption optBus("bus", "service bus address", "bus");
    optBus.setFlags(QCommandLineOption::HiddenFromHelp);

    parser.addOptions({optBus});
    parser.parse(QCoreApplication::arguments());

    QScopedPointer<QDBusServer> dbusServer;
    if (parser.isSet(optBus)) {
        auto busAddress = parser.value(optBus);
        dbusServer.reset(new QDBusServer(busAddress));
        if (!dbusServer->isConnected()) {
            qCritical() << "dbusServer is not connected" << dbusServer->lastError();
            return -1;
        }
        QObject::connect(dbusServer.data(), &QDBusServer::newConnection, [&systemHelper](const QDBusConnection &conn) {
            // FIXME: work round to keep conn alive, but we finally need to free clientConn.
            auto clientConn = new QDBusConnection(conn);
            registerServiceAndObject(clientConn, "",
                                     {
                                         {linglong::SystemHelperDBusPath, &systemHelper},
                                     });
        });
    } else {
        QDBusConnection bus = QDBusConnection::systemBus();
        if (!registerServiceAndObject(&bus, linglong::SystemHelperDBusName,
                                      {
                                          {linglong::SystemHelperDBusPath, &systemHelper},
                                      })) {
            return -1;
        }
    }

    return app.exec();
}