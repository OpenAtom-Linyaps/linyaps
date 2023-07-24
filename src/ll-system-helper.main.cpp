/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/dbus_gen_filesystem_helper_adaptor.h"
#include "module/dbus_gen_package_manager_helper_adaptor.h"
#include "module/dbus_ipc/dbus_common.h"
#include "module/dbus_ipc/dbus_system_helper_common.h"
#include "system_helper/filesystem_helper.h"
#include "system_helper/package_manager_helper.h"
#include "system_helper/privilege/privilege_install_portal.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

int main(int argc, char *argv[])
{
    using namespace linglong::system::helper;
    QCoreApplication app(argc, argv);

    PackageManagerHelper packageManagerHelper;
    PackageManagerHelperAdaptor packageManagerHelperAdaptor(&packageManagerHelper);

    FilesystemHelper filesystemHelper;
    FilesystemHelperAdaptor filesystemHelperAdaptor(&filesystemHelper);

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
                         [&packageManagerHelper](const QDBusConnection &conn) {
                             // FIXME: work round to keep conn alive, but we finally need to free
                             // clientConn.
                             const auto clientConn = new QDBusConnection(conn);
                             linglong::registerServiceAndObject(
                                     clientConn,
                                     "",
                                     {
                                             { linglong::PackageManagerHelperDBusPath,
                                               &packageManagerHelper },
                                     });
                         });
    } else {
        QDBusConnection bus = QDBusConnection::systemBus();
        if (!linglong::registerServiceAndObject(
                    &bus,
                    linglong::SystemHelperDBusServiceName,
                    {
                            { linglong::PackageManagerHelperDBusPath, &packageManagerHelper },
                            { linglong::FilesystemHelperDBusPath, &filesystemHelper },
                    })) {
            return -1;
        }
        qDebug() << "register" << linglong::SystemHelperDBusServiceName;
    }

    return app.exec();
}
