/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/adaptors/job_manager/job_manager1.h"
#include "linglong/adaptors/package_manager/package_manager1.h"
#include "linglong/api/dbus/v1/package_manager_helper.h"
#include "linglong/builder/builder_config.h"
#include "linglong/dbus_ipc/workaround.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/configure.h"
#include "linglong/util/connection.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>

using namespace linglong::utils::global;
using namespace linglong::utils::dbus;
using namespace linglong::builder;

namespace {
void withDBusDaemon()
{
    auto pkgManHelper =
      new linglong::api::dbus::v1::PackageManagerHelper("",
                                                        "/org/deepin/linglong/PackageManagerHelper",
                                                        QDBusConnection::systemBus(),
                                                        QCoreApplication::instance());

    auto config = linglong::repo::loadConfig(
      { linglong::util::getLinglongRootPath() + "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        qCritical() << config.error();
        QCoreApplication::exit(-1);
        return;
    }

    auto api = new linglong::api::client::ClientApi;
    api->setTimeOut(5000);
    api->setNewServerForAllOperations(
      QString::fromStdString(config->repos.at(config->defaultRepo)));
    api->setParent(QCoreApplication::instance());

    linglong::util::Connection dbConnection;
    auto ostreeRepo = new linglong::repo::OSTreeRepo(linglong::util::getLinglongRootPath(),
                                                     *config,
                                                     *api,
                                                     dbConnection);
    ostreeRepo->setParent(QCoreApplication::instance());

    auto client = new linglong::repo::RepoClient(*api);
    client->setParent(QCoreApplication::instance());

    auto packageManager = new linglong::service::PackageManager(*pkgManHelper,
                                                                *ostreeRepo,
                                                                *client,
                                                                QCoreApplication::instance());
    auto packageManagerAdaptor =
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

    auto jobMan = new linglong::job_manager::JobManager(*ostreeRepo, QCoreApplication::instance());
    auto jobManAdaptor = new linglong::adaptors::job_manger::JobManager1(jobMan);
    result = registerDBusObject(conn, "/org/deepin/linglong/JobManager", jobMan);
    if (!result.has_value()) {
        qCritical().noquote() << "Launching failed:" << Qt::endl << result.error().message();
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
        unregisterDBusObject(conn, "/org/deepin/linglong/JobManager");
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
    auto pkgManHelperConn =
      QDBusConnection::connectToPeer("unix:path=/tmp/linglong-system-helper.socket",
                                     "ll-system-helper");
    auto pkgManHelper =
      new linglong::api::dbus::v1::PackageManagerHelper("",
                                                        "/org/deepin/linglong/PackageManagerHelper",
                                                        pkgManHelperConn,
                                                        QCoreApplication::instance());

    auto config = linglong::repo::loadConfig(
      { linglong::util::getLinglongRootPath() + "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        qCritical() << config.error();
        QCoreApplication::exit(-1);
        return;
    }

    auto api = new linglong::api::client::ClientApi;
    api->setParent(QCoreApplication::instance());
    api->setNewServerForAllOperations(
      QString::fromStdString(config->repos.at(config->defaultRepo)));
    linglong::util::Connection dbConnection;
    auto ostreeRepo = new linglong::repo::OSTreeRepo(linglong::util::getLinglongRootPath(),
                                                     *config,
                                                     *api,
                                                     dbConnection);
    ostreeRepo->setParent(QCoreApplication::instance());

    auto client = new linglong::repo::RepoClient(*api);
    client->setParent(QCoreApplication::instance());

    auto packageManager = new linglong::service::PackageManager(*pkgManHelper,
                                                                *ostreeRepo,
                                                                *client,
                                                                QCoreApplication::instance());
    new linglong::adaptors::package_manger::PackageManager1(packageManager);

    auto jobMan = new linglong::job_manager::JobManager(*ostreeRepo, QCoreApplication::instance());
    new linglong::adaptors::job_manger::JobManager1(jobMan);

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

    QObject::connect(
      server,
      &QDBusServer::newConnection,
      [packageManager, jobMan](QDBusConnection conn) {
          auto res =
            registerDBusObject(conn, "/org/deepin/linglong/PackageManager", packageManager);
          if (!res.has_value()) {
              qCritical() << res.error().code() << res.error().message();
              return;
          }
          QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn]() {
              unregisterDBusObject(conn, "/org/deepin/linglong/PackageManager");
          });

          res = registerDBusObject(conn, "/org/deepin/linglong/JobManager", jobMan);
          if (!res.has_value()) {
              qCritical() << res.error().code() << res.error().message();
              return;
          }
          QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn]() {
              unregisterDBusObject(conn, "/org/deepin/linglong/JobManager");
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
          registerDBusParam();

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
