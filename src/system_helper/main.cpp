/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/dbus_ipc/dbus_common.h"
#include "module/dbus_ipc/dbus_system_helper_common.h"
#include "module/systemhelperadaptor.h"
#include "module/util/log/log_handler.h"
#include "privilege/privilege_install_portal.h"
#include "system_helper.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
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

    parser.addOptions({ optBus });
    parser.parse(QCoreApplication::arguments());

    QScopedPointer<QDBusServer> dbusServer;
    if (parser.isSet(optBus)) {
        const auto busAddress = parser.value(optBus);
        dbusServer.reset(new QDBusServer(busAddress));
        if (!dbusServer->isConnected()) {
            qCritical() << "dbusServer is not connected" << dbusServer->lastError();
            return -1;
        }
        QObject::connect(dbusServer.data(),
                         &QDBusServer::newConnection,
                         [&systemHelper](const QDBusConnection &conn) {
                             // FIXME: work round to keep conn alive, but we finally need to free
                             // clientConn.
                             const auto clientConn = new QDBusConnection(conn);
                             linglong::registerServiceAndObject(
                                     clientConn,
                                     "",
                                     {
                                             { linglong::SystemHelperDBusPath, &systemHelper },
                                     });
                         });
    } else {
        QDBusConnection bus = QDBusConnection::systemBus();
        if (!linglong::registerServiceAndObject(
                    &bus,
                    linglong::SystemHelperDBusName,
                    {
                            { linglong::SystemHelperDBusPath, &systemHelper },
                    })) {
            return -1;
        }
    }

    return app.exec();
}
