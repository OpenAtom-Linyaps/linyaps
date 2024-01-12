/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/adaptors/system_helper/filesystem_helper1.h"
#include "linglong/adaptors/system_helper/package_manager_helper1.h"
#include "linglong/dbus_ipc/dbus_system_helper_common.h"
#include "linglong/system_helper/filesystem_helper.h"
#include "linglong/system_helper/package_manager_helper.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>

using namespace linglong::utils::global;
using namespace linglong::utils::dbus;
using namespace linglong::system::helper;

namespace {

void withDBusDaemon()
{
    auto packageManagerHelper =
      new linglong::system::helper::PackageManagerHelper(QCoreApplication::instance());
    auto packageManagerHelperAdaptor =
      new linglong::adaptors::system_helper::PackageManagerHelper1(packageManagerHelper);

    auto filesystemHelper =
      new linglong::system::helper::FilesystemHelper(QCoreApplication::instance());
    auto filesystemHelperAdaptor =
      new linglong::adaptors::system_helper::FilesystemHelper1(filesystemHelper);

    QDBusConnection bus = QDBusConnection::systemBus();

    auto res =
      registerDBusObject(bus, linglong::PackageManagerHelperDBusPath, packageManagerHelper);
    if (!res.has_value()) {
        qCritical() << res.error().code() << res.error().message();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [bus]() {
        unregisterDBusObject(bus, linglong::PackageManagerHelperDBusPath);
    });

    res = registerDBusObject(bus, linglong::FilesystemHelperDBusPath, filesystemHelper);
    if (!res.has_value()) {
        qCritical() << res.error().code() << res.error().message();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [bus]() {
        unregisterDBusObject(bus, linglong::FilesystemHelperDBusPath);
    });

    res = registerDBusService(bus, linglong::SystemHelperDBusServiceName);
    if (!res.has_value()) {
        qCritical() << res.error();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [bus]() {
        auto res = unregisterDBusService(bus, linglong::SystemHelperDBusServiceName);
        if (!res.has_value()) {
            qCritical() << res.error().code() << res.error().message();
        }
    });
}

void withoutDBusDaemon()
{
    qInfo() << "Running linglong system helper without dbus daemon...";

    auto packageManagerHelper =
      new linglong::system::helper::PackageManagerHelper(QCoreApplication::instance());
    auto packageManagerHelperAdaptor =
      new linglong::adaptors::system_helper::PackageManagerHelper1(packageManagerHelper);

    auto filesystemHelper =
      new linglong::system::helper::FilesystemHelper(QCoreApplication::instance());
    auto filesystemHelperAdaptor =
      new linglong::adaptors::system_helper::FilesystemHelper1(filesystemHelper);

    auto qDBusServer =
      new QDBusServer("unix:path=/tmp/linglong-system-helper.socket", QCoreApplication::instance());
    if (!qDBusServer->isConnected()) {
        qCritical() << "dbusServer is not connected:" << qDBusServer->lastError();
        QCoreApplication::exit(-1);
        return;
    }

    // NOTE(black_desk):
    // We should allow anonymous auth while using --no-dbus mode,
    // as that is user deepin-linglong who is
    // connecting to this dbus server in p2p mode.
    // That connection will be closed immediately in libdbus
    // if we do not allow anonymous auth.
    // I have no idea about what is going on in that library.
    // It seems not to be a security issue,
    // as you need to get root at first place to run linglong-system-helper.
    qDBusServer->setAnonymousAuthenticationAllowed(true);

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
        if (QDir::root().remove("/tmp/linglong-system-helper.socket")) {
            return;
        }
        qCritical() << "Failed to remove /tmp/linglong-system-helper.socket.";
    });

    QObject::connect(
      qDBusServer,
      &QDBusServer::newConnection,
      [packageManagerHelper, filesystemHelper](QDBusConnection conn) {
          auto ret =
            registerDBusObject(conn, linglong::PackageManagerHelperDBusPath, packageManagerHelper);
          if (!ret.has_value()) {
              qCritical() << ret.error().code() << ret.error().message();
          } else {
              QObject::connect(QCoreApplication::instance(),
                               &QCoreApplication::aboutToQuit,
                               [conn]() {
                                   unregisterDBusObject(conn,
                                                        linglong::PackageManagerHelperDBusPath);
                               });
          }

          ret = registerDBusObject(conn, linglong::FilesystemHelperDBusPath, filesystemHelper);
          if (!ret.has_value()) {
              qCritical() << ret.error().code() << ret.error().message();
          } else {
              QObject::connect(QCoreApplication::instance(),
                               &QCoreApplication::aboutToQuit,
                               [conn]() {
                                   unregisterDBusObject(conn, linglong::FilesystemHelperDBusPath);
                               });
          }
      });
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    applicationInitializte();

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      []() {
          QCommandLineParser parser;
          QCommandLineOption optBus("no-dbus", "run without dbus daemon", "no-dbus");
          optBus.setFlags(QCommandLineOption::HiddenFromHelp);

          parser.addOptions({ optBus });
          parser.parse(QCoreApplication::arguments());

          if (parser.isSet(optBus)) {
              withoutDBusDaemon();
              return;
          }

          withDBusDaemon();
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
