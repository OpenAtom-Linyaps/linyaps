/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/adaptors/package_manager/package_manager1.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>

using namespace linglong::utils::global;
using namespace linglong::utils::dbus;

namespace {
void withDBusDaemon()
{
    auto config = linglong::repo::loadConfig(
      { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        qCritical() << config.error();
        QCoreApplication::exit(-1);
        return;
    }

    qWarning() << "server" << config->repos[config->defaultRepo].c_str();
    auto clientFactory = new linglong::repo::ClientFactory(config->repos[config->defaultRepo]);
    clientFactory->setParent(QCoreApplication::instance());
    auto ostreeRepo = new linglong::repo::OSTreeRepo(QDir(LINGLONG_ROOT), *config, *clientFactory);
    ostreeRepo->setParent(QCoreApplication::instance());

    auto packageManager =
      new linglong::service::PackageManager(*ostreeRepo, QCoreApplication::instance());
    new linglong::adaptors::package_manger::PackageManager1(packageManager);

    QDBusConnection conn = QDBusConnection::systemBus();
    auto result = registerDBusObject(conn, "/org/deepin/linglong/PackageManager", packageManager);
    if (!result.has_value()) {
        qCritical().noquote() << "Launching failed:" << Qt::endl << result.error().message();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
        unregisterDBusObject(conn, "/org/deepin/linglong/PackageManager");
    });

    result = registerDBusService(conn, "org.deepin.linglong.PackageManager");
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
        auto result = unregisterDBusService(conn,
                                            // FIXME: use cmake option
                                            "org.deepin.linglong.PackageManager");
        if (!result.has_value()) {
            qWarning().noquote() << "During exiting:" << Qt::endl << result.error().message();
        }
    });
    if (!result.has_value()) {
        qCritical().noquote() << "Launching failed:" << Qt::endl << result.error().message();
        QCoreApplication::exit(-1);
        return;
    }

    return;
}

void withoutDBusDaemon()
{
    qInfo() << "Running linglong package manager without dbus daemon...";

    auto config = linglong::repo::loadConfig(
      { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        qCritical() << config.error();
        QCoreApplication::exit(-1);
        return;
    }
    auto clientFactory = new linglong::repo::ClientFactory(config->repos[config->defaultRepo]);
    clientFactory->setParent(QCoreApplication::instance());
    auto ostreeRepo = new linglong::repo::OSTreeRepo(QDir(LINGLONG_ROOT), *config, *clientFactory);
    ostreeRepo->setParent(QCoreApplication::instance());

    auto packageManager =
      new linglong::service::PackageManager(*ostreeRepo, QCoreApplication::instance());
    new linglong::adaptors::package_manger::PackageManager1(packageManager);

    auto server = new QDBusServer("unix:path=/tmp/linglong-package-manager.socket",
                                  QCoreApplication::instance());
    if (!server->isConnected()) {
        qCritical() << "listen on socket:" << server->lastError();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
        if (QDir::root().remove("/tmp/linglong-package-manager.socket")) {
            return;
        }
        qCritical() << "failed to remove /tmp/linglong-package-manager.socket.";
    });

    QObject::connect(server, &QDBusServer::newConnection, [packageManager](QDBusConnection conn) {
        auto res = registerDBusObject(conn, "/org/deepin/linglong/PackageManager", packageManager);
        if (!res.has_value()) {
            qCritical() << res.error().code() << res.error().message();
            return;
        }
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn]() {
            unregisterDBusObject(conn, "/org/deepin/linglong/PackageManager");
        });
    });
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    QCoreApplication app(argc, argv);

    applicationInitializte();

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      []() {
          QString user = qgetenv("USER");
          if (user != LINGLONG_USERNAME) {
              QCoreApplication::exit(-1);
              return;
          }

          QCommandLineParser parser;
          QCommandLineOption optBus("no-dbus", "service without dbus-daemon");
          optBus.setFlags(QCommandLineOption::HiddenFromHelp);

          parser.addOptions({ optBus });
          parser.parse(QCoreApplication::arguments());

          if (!parser.isSet(optBus)) {
              withDBusDaemon();
              return;
          }

          withoutDBusDaemon();
          return;
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
} // namespace
