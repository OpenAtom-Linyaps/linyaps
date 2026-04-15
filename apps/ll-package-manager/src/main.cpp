/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
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
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/cli/crun/Crun.hpp"

#include <CLI/CLI.hpp>

#include <QCoreApplication>

#include <filesystem>
#include <memory>
#include <string>

namespace {

struct CommandLineOptions
{
    bool noDBus{ false };
    std::string initRunContext;
    std::string containerID;
};

auto parseCommandLine(int argc, char *argv[], CommandLineOptions &options) -> int
{
    CLI::App commandParser{ "linyaps package manager" };
    constexpr auto cliHiddenGroup = "";

    auto *noDBusOption =
      commandParser.add_flag("--no-dbus", options.noDBus, "service without dbus-daemon")
        ->group(cliHiddenGroup);

    auto *initRunOption = commandParser.add_option("--init-run",
                                                   options.initRunContext,
                                                   "json string of RunContextConfig");
    auto *containerIDOption = commandParser.add_option("--id", options.containerID, "container id");

    initRunOption->needs(containerIDOption);
    containerIDOption->needs(initRunOption);
    initRunOption->excludes(noDBusOption);
    containerIDOption->excludes(noDBusOption);

    try {
        commandParser.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return commandParser.exit(e);
    }

    return 0;
}

auto createRepoForDaemon(bool migrateRepo)
  -> linglong::utils::error::Result<std::unique_ptr<linglong::repo::OSTreeRepo>>
{
    LINGLONG_TRACE("create repo for daemon");

    auto config = linglong::repo::loadConfig(
      std::vector<std::filesystem::path>{ LINGLONG_ROOT "/config.yaml",
                                          LINGLONG_DATA_DIR "/config.yaml" });
    if (!config.has_value()) {
        return LINGLONG_ERR("load config failed", std::move(config));
    }

    std::filesystem::path repoRoot(LINGLONG_ROOT);
    auto res = linglong::utils::ensureDirectory(repoRoot);
    if (!res) {
        return LINGLONG_ERR("failed to create repository directory", std::move(res));
    }

    if (migrateRepo) {
        auto ret = linglong::repo::tryMigrate(LINGLONG_ROOT, *config);
        if (ret == linglong::repo::MigrateResult::Failed) {
            return LINGLONG_ERR("failed to migrate repository");
        }
    }

    auto repo = linglong::repo::OSTreeRepo::create(repoRoot, *config);
    if (!repo) {
        return LINGLONG_ERR("failed to create repo", std::move(repo));
    }

    auto repoInstance = std::move(repo).value();
    auto result = repoInstance->fixExportAllEntries();
    if (!result.has_value()) {
        LogE("fix export all entries failed: {}", result.error());
    }

    return repoInstance;
}

auto createPackageManager(std::unique_ptr<linglong::repo::OSTreeRepo> repo,
                          ocppi::cli::CLI &cli,
                          QObject *parent) -> std::unique_ptr<linglong::service::PackageManager>
{
    LINGLONG_TRACE("create package manager");

    return std::make_unique<linglong::service::PackageManager>(
      std::move(repo),
      std::make_unique<linglong::runtime::ContainerBuilder>(cli),
      parent);
}

auto runDaemonMode(ocppi::cli::CLI &cli, const CommandLineOptions &options) -> int
{
    LINGLONG_TRACE("run daemon mode");

    auto repo = createRepoForDaemon(!options.noDBus);
    if (!repo) {
        LogE("{}", repo.error());
        return -1;
    }

    auto packageManager =
      createPackageManager(std::move(repo).value(), cli, QCoreApplication::instance());
    packageManager->initDaemonMode();
    auto *packageManagerPtr = packageManager.get();
    new linglong::adaptors::package_manger::PackageManager1(packageManagerPtr);

    if (!options.noDBus) {
        QDBusConnection conn = QDBusConnection::systemBus();
        auto result =
          linglong::common::dbus::registerDBusObject(conn,
                                                     "/org/deepin/linglong/PackageManager1",
                                                     packageManagerPtr);
        if (!result.has_value()) {
            LogE("register dbus object failed: {}", result.error());
            return -1;
        }
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
            linglong::common::dbus::unregisterDBusObject(conn,
                                                         "/org/deepin/linglong/PackageManager1");
        });

        result =
          linglong::common::dbus::registerDBusService(conn, "org.deepin.linglong.PackageManager1");
        if (!result.has_value()) {
            LogE("register dbus service failed: {}", result.error());
            return -1;
        }
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [conn] {
            auto result =
              linglong::common::dbus::unregisterDBusService(conn,
                                                            "org.deepin.linglong.PackageManager1");
            if (!result.has_value()) {
                LogW("unregister dbus service failed: {}", result.error());
            }
        });

    } else {
        LogI("Running linglong package manager without dbus daemon...");

        auto *server = new QDBusServer("unix:path=/tmp/linglong-package-manager.socket",
                                       QCoreApplication::instance());
        if (!server->isConnected()) {
            LogE("listen on socket: {}", server->lastError().message().toStdString());
            return -1;
        }
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() {
            if (QDir::root().remove("/tmp/linglong-package-manager.socket")) {
                return;
            }
            LogE("failed to remove /tmp/linglong-package-manager.socket.");
        });

        QObject::connect(server,
                         &QDBusServer::newConnection,
                         [packageManagerPtr](QDBusConnection conn) {
                             auto res = linglong::common::dbus::registerDBusObject(
                               conn,
                               "/org/deepin/linglong/PackageManager1",
                               packageManagerPtr);
                             if (!res.has_value()) {
                                 LogE("register dbus object failed: {}", res.error());
                                 return;
                             }
                             QObject::connect(QCoreApplication::instance(),
                                              &QCoreApplication::aboutToQuit,
                                              [conn]() {
                                                  linglong::common::dbus::unregisterDBusObject(
                                                    conn,
                                                    "/org/deepin/linglong/PackageManager1");
                                              });
                         });
    }

    packageManager.release();
    return 0;
}

auto runInitRunMode(ocppi::cli::CLI &cli, const CommandLineOptions &options) -> int
{
    LogD("runInitRunMode");

    auto repo = linglong::repo::OSTreeRepo::loadFromPath(LINGLONG_ROOT);
    if (!repo) {
        LogE("failed to load repo: {}", repo.error());
        return -1;
    }
    auto packageManager =
      createPackageManager(std::move(repo).value(), cli, QCoreApplication::instance());

    auto ret = packageManager->initRunContext(options.initRunContext, options.containerID);
    if (!ret) {
        LogE("init run context failed: {}", ret.error());
        return -1;
    }

    return 0;
}

} // namespace

auto main(int argc, char *argv[]) -> int
{
    QCoreApplication app(argc, argv);

    linglong::common::global::applicationInitialize();
    linglong::common::global::initLinyapsLogSystem(linglong::utils::log::LogBackend::Journal);

    CommandLineOptions options;
    auto parseResult = parseCommandLine(argc, argv, options);
    if (parseResult != 0) {
        return parseResult;
    }

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
      [&ociRuntime, &options]() {
          QString user = qgetenv("USER");
          if (user != LINGLONG_USERNAME) {
              LogE("PM is designed to run as {}", LINGLONG_USERNAME);
              QCoreApplication::exit(-1);
              return;
          }

          if (!options.initRunContext.empty()) {
              QCoreApplication::exit(runInitRunMode(**ociRuntime, options));
              return;
          }

          auto ret = runDaemonMode(**ociRuntime, options);
          if (ret != 0) {
              QCoreApplication::exit(ret);
          }
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
