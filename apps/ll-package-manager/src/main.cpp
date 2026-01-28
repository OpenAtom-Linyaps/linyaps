/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "configure.h"
#include "linglong/adaptors/package_manager/package_manager1.h"
#include "linglong/common/dbus/register.h"
#include "linglong/common/global/initialize.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/config.h"
#include "linglong/repo/migrate.h"
#include "linglong/repo/ostree_repo.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/cli/crun/Crun.hpp"

#include <QCoreApplication>

#include <filesystem>

namespace {
void withDBusDaemon(ocppi::cli::CLI &cli)
{
    auto config = linglong::repo::loadConfig(
      { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        LogE("load config failed: {}", config.error());
        QCoreApplication::exit(-1);
        return;
    }

    auto repoRoot = QDir(LINGLONG_ROOT);
    if (!repoRoot.exists() && !repoRoot.mkpath(".")) {
        LogE("failed to create repository directory {}", repoRoot.absolutePath().toStdString());
        std::abort();
    }

    auto ret = linglong::repo::tryMigrate(LINGLONG_ROOT, *config);
    if (ret == linglong::repo::MigrateResult::Failed) {
        LogE("failed to migrate repository");
        QCoreApplication::exit(-1);
    }
    auto *ostreeRepo = new linglong::repo::OSTreeRepo(repoRoot, *config);
    ostreeRepo->setParent(QCoreApplication::instance());
    auto result = ostreeRepo->fixExportAllEntries();
    if (!result.has_value()) {
        LogE("fix export all entries failed: {}", result.error());
    }

    auto *containerBuilder = new linglong::runtime::ContainerBuilder(cli);
    containerBuilder->setParent(QCoreApplication::instance());

    QDBusConnection conn = QDBusConnection::systemBus();
    auto *packageManager = new linglong::service::PackageManager(*ostreeRepo,
                                                                 *containerBuilder,
                                                                 QCoreApplication::instance());
    new linglong::adaptors::package_manger::PackageManager1(packageManager);
    result = linglong::common::dbus::registerDBusObject(conn,
                                                        "/org/deepin/linglong/PackageManager1",
                                                        packageManager);
    if (!result.has_value()) {
        LogE("register dbus object failed: {}", result.error());
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
        linglong::common::dbus::unregisterDBusObject(conn, "/org/deepin/linglong/PackageManager1");
    });

    result =
      linglong::common::dbus::registerDBusService(conn, "org.deepin.linglong.PackageManager1");
    if (!result.has_value()) {
        LogE("register dbus service failed: {}", result.error());
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
        auto result =
          linglong::common::dbus::unregisterDBusService(conn,
                                                        // FIXME: use cmake option
                                                        "org.deepin.linglong.PackageManager1");
        if (!result.has_value()) {
            LogW("unregister dbus service failed: {}", result.error());
        }
    });
}

void withoutDBusDaemon(ocppi::cli::CLI &cli)
{
    LogI("Running linglong package manager without dbus daemon...");

    auto config = linglong::repo::loadConfig(
      { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        LogE("load config failed: {}", config.error());
        QCoreApplication::exit(-1);
        return;
    }

    auto repoRoot = QDir(LINGLONG_ROOT);
    if (!repoRoot.exists() && !repoRoot.mkpath(".")) {
        LogE("failed to create repository directory {}", repoRoot.absolutePath().toStdString());
        std::abort();
    }

    auto *ostreeRepo = new linglong::repo::OSTreeRepo(repoRoot, *config);
    ostreeRepo->setParent(QCoreApplication::instance());
    auto result = ostreeRepo->fixExportAllEntries();
    if (!result.has_value()) {
        LogE("fix export all entries failed: {}", result.error());
    }

    auto *containerBuilder = new linglong::runtime::ContainerBuilder(cli);
    containerBuilder->setParent(QCoreApplication::instance());

    auto packageManager = new linglong::service::PackageManager(*ostreeRepo,
                                                                *containerBuilder,
                                                                QCoreApplication::instance());
    new linglong::adaptors::package_manger::PackageManager1(packageManager);

    auto server = new QDBusServer("unix:path=/tmp/linglong-package-manager.socket",
                                  QCoreApplication::instance());
    if (!server->isConnected()) {
        LogE("listen on socket: {}", server->lastError().message().toStdString());
        QCoreApplication::exit(-1);
        return;
    }
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
        if (QDir::root().remove("/tmp/linglong-package-manager.socket")) {
            return;
        }
        LogE("failed to remove /tmp/linglong-package-manager.socket.");
    });

    QObject::connect(server, &QDBusServer::newConnection, [packageManager](QDBusConnection conn) {
        auto res =
          linglong::common::dbus::registerDBusObject(conn,
                                                     "/org/deepin/linglong/PackageManager1",
                                                     packageManager);
        if (!res.has_value()) {
            LogE("register dbus object failed: {}", res.error());
            return;
        }
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn]() {
            linglong::common::dbus::unregisterDBusObject(conn,
                                                         "/org/deepin/linglong/PackageManager1");
        });
    });
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    QCoreApplication app(argc, argv);

    linglong::common::global::applicationInitialize();
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Journal);

    auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
    if (ociRuntimeCLI.isEmpty()) {
        ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
    }

    auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
    if (path.isEmpty()) {
        LogE("{} not found", ociRuntimeCLI.toStdString());
        return -1;
    }

    auto ociRuntime = ocppi::cli::crun::Crun::New(path.toStdString());
    if (!ociRuntime) {
        std::rethrow_exception(ociRuntime.error());
    }

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [&ociRuntime]() {
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
              withDBusDaemon(**ociRuntime);
              return;
          }

          withoutDBusDaemon(**ociRuntime);
          return;
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
} // namespace
