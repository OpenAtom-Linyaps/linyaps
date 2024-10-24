/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/dbus_notifier.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/json_printer.h"
#include "linglong/cli/terminal_notifier.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <CLI/CLI.hpp>

#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <cstddef>
#include <functional>
#include <memory>

#include <unistd.h>
#include <wordexp.h>

using namespace linglong::utils::error;
using namespace linglong::package;
using namespace linglong::cli;

namespace {

void startProcess(const QString &program, const QStringList &args = {})
{
    QProcess process;
    auto envs = process.environment();
    envs.push_back("QT_FORCE_STDERR_LOGGING=1");
    process.setEnvironment(envs);
    process.setProgram(program);
    process.setArguments(args);

    qint64 pid = 0;
    process.startDetached(&pid);

    qDebug() << "Start" << program << args << "as" << pid;

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [pid]() {
        qDebug() << "Kill" << pid;
        kill(pid, SIGTERM);
    });
}

std::vector<std::string> transformOldExec(int argc, char **argv) noexcept
{
    std::vector<std::string> res{ argv + 1, argv + argc };
    if (std::find(res.begin(), res.end(), "run") == res.end()) {
        return res;
    }

    auto exec = std::find(res.begin(), res.end(), "--exec");

    if (exec == res.end()) {
        return res;
    }

    if ((exec + 1) == res.end() || (exec + 2) != res.end()) {
        *exec = "--";
        qDebug() << "replace `--exec` with `--`";
        return res;
    }

    wordexp_t words;
    auto _ = linglong::utils::finally::finally([&]() {
        wordfree(&words);
    });

    auto ret = wordexp((exec + 1)->c_str(), &words, 0);
    if (ret) {
        qCritical() << "wordexp on" << (exec + 1)->c_str() << "failed with" << ret
                    << "transform old exec arguments failed.";
        return res;
    }

    res.erase(exec, res.end());
    res.emplace_back("--");

    for (size_t i = 0; i < words.we_wordc; i++) {
        res.emplace_back(words.we_wordv[i]);
    }

    QStringList list{};

    for (const auto &arg : res) {
        list.push_back(QString::fromStdString(arg));
    }

    qDebug() << "using new args" << list;
    return res;
}

} // namespace

using namespace linglong::utils::global;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    applicationInitializte();

    bool versionFlag = false, jsonFlag = false, noDBus = false;

    CLI::App commandParser{ R"(linglong CLI
A CLI program to run application and manage linglong pagoda and tiers.)" };
    commandParser.set_help_all_flag("--help-all", "Expand all help");

    // add flags
    commandParser.add_flag("--version", versionFlag, "Show version.")->ignore_case();
    commandParser
      .add_flag(
        "--no-dbus",
        noDBus,
        "Use peer to peer DBus, this is used only in case that DBus daemon is not available.")
      ->ignore_case()
      ->group(""); // group name empty for hidden --no-dbus flag
    commandParser.add_flag("--json", jsonFlag, R"(Use json to output command result.)")
      ->ignore_case();

    CLI::Validator validatorString{
        [](std::string &parameter) {
            if (parameter.empty()) {
                return std::string{
                    "input parameter is empty, please input valid parameter instead\n"
                };
            }
            return std::string();
        },
        ""
    };

    // add sub command run
    std::string path, appid, file, url;
    std::vector<std::string> commands;
    auto cliRun = commandParser.add_subcommand("run", "Run an application.")->ignore_case();

    // add sub command run options
    cliRun->add_option("APP", appid, "Specify the application.")
      ->required()
      ->check(validatorString);
    cliRun->add_option("--dbus-proxy-cfg", path, "Path of config of linglong-dbus-proxy.")
      ->check(CLI::ExistingPath);
    cliRun
      ->add_option(
        "--file",
        file,
        "you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.")
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    cliRun
      ->add_option(
        "--url",
        url,
        "you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.")
      ->type_name("URL");
    cliRun->add_option("COMMAND", commands, "command will run in container.")->option_text("--");

    // add sub command ps
    auto cliPs = commandParser.add_subcommand("ps", "List all pagodas.")->ignore_case();

    // add sub command exec
    std::string pagoda, dir;
    auto cliExec =
      commandParser.add_subcommand("exec", "Execute command in a pagoda.")->ignore_case();
    cliExec->add_option("PAGODA", pagoda, "Specify the pagodas (container).")
      ->check(validatorString);
    cliExec->add_option("--working-directory", dir, "Specify working directory.")
      ->type_name("PATH")
      ->check(CLI::ExistingDirectory);
    cliExec->add_option("COMMAND", commands, "command will run in container.")->option_text("--");

    // add sub command enter
    auto cliEnter = commandParser.add_subcommand("enter", "Enter a pagoda.")->ignore_case();
    cliEnter->add_option("PAGODA", pagoda, "Specify the pagodas (container).");
    cliEnter->add_option("--working-directory", dir, "Specify working directory.")
      ->type_name("PATH")
      ->check(CLI::ExistingDirectory);
    cliEnter->add_option("COMMAND", commands, "command will run in container.")->option_text("--");

    // add sub command kill
    auto cliKill = commandParser.add_subcommand("kill", "Stop applications and remove the pagoda.")
                     ->ignore_case();
    cliKill->add_option("PAGODA", pagoda, "Specify the pagodas (container).")
      ->check(validatorString);

    // add sub command install
    std::string tier, module;
    auto cliInstall = commandParser.add_subcommand("install", "Install tier(s).")->ignore_case();
    cliInstall->add_option("TIER", tier, "Specify the tier (container layer).")
      ->check(validatorString);
    cliInstall->add_option("--module", module, "Install a specify module.")->type_name("MODULE")
      ->check(validatorString);

    // add sub command uninstall
    auto cliUninstall =
      commandParser.add_subcommand("uninstall", "Uninstall tier(s).")->ignore_case();
    cliUninstall->add_option("TIER", tier, "Specify the tier (container layer).")
      ->check(validatorString);
    cliUninstall->add_option("--module", module, "Uninstall a specify module.")->type_name("MODULE")
      ->check(validatorString);

    // add sub command upgrade
    auto cliUpgrade = commandParser.add_subcommand("upgrade", "Upgrade tier(s).")->ignore_case();
    cliUpgrade->add_option("TIER", tier, "Specify the tier (container layer).")
      ->check(validatorString);

    // add sub command search
    std::string type{ "app" };
    bool showDevel = false;
    auto cliSearch = commandParser.add_subcommand("search", "Search for tiers.")->ignore_case();
    cliSearch->add_option("TIER", tier, "Specify the tier (container layer).")
      ->check(validatorString);
    cliSearch
      ->add_option(
        "--type",
        type,
        R"(Filter result with tiers type. One of "runtime", "app" or "all". [default: app])")
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliSearch->add_flag("--dev", showDevel, "include develop tiers in result.");

    // add sub command list
    auto cliList = commandParser.add_subcommand("list", "Uninstall tier(s).")->ignore_case();
    cliList
      ->add_option(
        "--type",
        type,
        R"(Filter result with tiers type. One of "runtime", "app" or "all". [default: app])")
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);

    // add sub command repo
    std::string name, repoUrl;
    auto cliRepo =
      commandParser
        .add_subcommand("repo", "Display or modify information of the repository currently using.")
        ->ignore_case();
    cliRepo->require_subcommand(1);

    // add repo sub command add
    auto repoAdd = cliRepo->add_subcommand("add", "Add a new repository.")->ignore_case();
    repoAdd->add_option("NAME", name, "Specify the repo name.")->required()->check(validatorString);
    repoAdd->add_option("URL", repoUrl, "Url of the repository.")
      ->required()
      ->check(validatorString);

    // add repo sub command modify
    auto repoModify = cliRepo->add_subcommand("modify", "Modify repository URL.")->ignore_case();
    repoModify->add_option("--name", name, "Specify the repo name.")
      ->type_name("REPO")
      ->check(validatorString);
    repoModify->add_option("URL", repoUrl, "Url of the repository.")
      ->required()
      ->check(validatorString);

    // add repo sub command remove
    auto repoRemove = cliRepo->add_subcommand("remove", "Remove a repository.")->ignore_case();
    repoRemove->add_option("NAME", name, "Specify the repo name.")
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto repoUpdate =
      cliRepo->add_subcommand("update", "Update to a new repository.")->ignore_case();
    repoUpdate->add_option("NAME", name, "Specify the repo name.")
      ->required()
      ->check(validatorString);
    repoUpdate->add_option("URL", repoUrl, "Url of the repository.")
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto repoSetDefault =
      cliRepo->add_subcommand("set-default", "Set default repository name.")->ignore_case();
    repoSetDefault->add_option("NAME", name, "Specify the repo name.")
      ->required()
      ->check(validatorString);

    // add repo sub command show
    auto repoShow = cliRepo->add_subcommand("show", "Show repository.")->ignore_case();

    // add sub command info
    auto cliInfo =
      commandParser.add_subcommand("info", "Display the information of layer.")->ignore_case();
    cliInfo->add_option("TIER", tier, "Specify the tier (container layer).")
      ->check(validatorString);

    // add sub command content
    auto cliContent =
      commandParser.add_subcommand("content", "Display the exported files of application.")
        ->ignore_case();
    cliContent->add_option("APP", appid, "Specify the application.")->check(validatorString);

    // auto raw_args = transformOldExec(argc, argv);

    CLI11_PARSE(commandParser, argc, argv);

    if (versionFlag) {
        std::cout << "linglong CLI version " << LINGLONG_VERSION << std::endl;
        return 0;
    }

    CliOptions options = linglong::cli::CliOptions(file,
                                                   url,
                                                   dir,
                                                   appid,
                                                   pagoda,
                                                   tier,
                                                   module,
                                                   type,
                                                   name,
                                                   repoUrl,
                                                   commands,
                                                   showDevel);

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [&argc, &argv, &commandParser, &noDBus, &jsonFlag, &options]() {
          auto repoRoot = QDir(LINGLONG_ROOT);
          if (!repoRoot.exists()) {
              qCritical() << "underlying repository doesn't exist:" << repoRoot.absolutePath();
              QCoreApplication::exit(-1);
              return;
          }

          auto pkgManConn = QDBusConnection::systemBus();

          auto msg = QDBusMessage::createMethodCall("org.deepin.linglong.PackageManager1",
                                                    "/org/deepin/linglong/Migrate1",
                                                    "org.deepin.linglong.Migrate1",
                                                    "WaitForAvailable");
          QDBusReply<void> migrateReply = pkgManConn.call(msg);
          if (migrateReply.isValid()) {
              qCritical()
                << "package manager is migrating underlying data, please try again later.";
              QCoreApplication::exit(-1);
              return;
          }

          auto *pkgMan =
            new linglong::api::dbus::v1::PackageManager("org.deepin.linglong.PackageManager1",
                                                        "/org/deepin/linglong/PackageManager1",
                                                        pkgManConn,
                                                        QCoreApplication::instance());
          if (noDBus) {
              if (getuid() != 0) {
                  qCritical() << "--no-dbus should only be used by root user.";
                  QCoreApplication::exit(-1);
                  return;
              }

              qInfo() << "some subcommands will failed in --no-dbus mode.";

              const auto pkgManAddress = QString("unix:path=/tmp/linglong-package-manager.socket");

              QThread::sleep(1);

              startProcess("sudo",
                           { "--user",
                             LINGLONG_USERNAME,
                             "--preserve-env=QT_FORCE_STDERR_LOGGING",
                             "--preserve-env=QDBUS_DEBUG",
                             LINGLONG_LIBEXEC_DIR "/ll-package-manager",
                             "--no-dbus" });
              QThread::sleep(1);

              pkgManConn = QDBusConnection::connectToPeer(pkgManAddress, "ll-package-manager");
              if (!pkgManConn.isConnected()) {
                  qCritical() << "Failed to connect to ll-package-manager:"
                              << pkgManConn.lastError();
                  QCoreApplication::exit(-1);
                  return;
              }

              pkgMan =
                new linglong::api::dbus::v1::PackageManager("",
                                                            "/org/deepin/linglong/PackageManager1",
                                                            pkgManConn,
                                                            QCoreApplication::instance());
          } else {
              // NOTE: We need to ping package manager to make it initialize system linglong
              // repository.
              auto peer = linglong::api::dbus::v1::DBusPeer("org.deepin.linglong.PackageManager1",
                                                            "/org/deepin/linglong/PackageManager1",
                                                            pkgManConn);
              auto reply = peer.Ping();
              reply.waitForFinished();
              if (!reply.isValid()) {
                  qCritical() << "Failed to activate org.deepin.linglong.PackageManager1"
                              << reply.error();
                  QCoreApplication::exit(-1);
                  return;
              }
          }

          std::unique_ptr<Printer> printer;
          if (jsonFlag) {
              printer = std::make_unique<JSONPrinter>();
          } else {
              printer = std::make_unique<Printer>();
          }

          auto config = linglong::repo::loadConfig(
            { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
          if (!config) {
              qCritical() << config.error();
              QCoreApplication::exit(-1);
              return;
          }
          linglong::repo::ClientFactory clientFactory(config->repos[config->defaultRepo]);

          auto *repo = new linglong::repo::OSTreeRepo(repoRoot, *config, clientFactory);
          repo->setParent(QCoreApplication::instance());

          auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
          if (ociRuntimeCLI.isEmpty()) {
              ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
          }

          auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
          if (path.isEmpty()) {
              qCritical() << ociRuntimeCLI << "not found";
              QCoreApplication::exit(-1);
              return;
          }
          auto ociRuntime = ocppi::cli::crun::Crun::New(path.toStdString());
          if (!ociRuntime) {
              std::rethrow_exception(ociRuntime.error());
          }
          auto *containerBuidler = new linglong::runtime::ContainerBuilder(**ociRuntime);
          containerBuidler->setParent(QCoreApplication::instance());

          std::unique_ptr<InteractiveNotifier> notifier{ nullptr };
          while (true) {
              try {
                  notifier = std::make_unique<DBusNotifier>();
                  break;
              } catch (std::runtime_error &err) {
                  qWarning() << "initialize DBus notifier error:" << err.what()
                             << "try to fallback to terminal notifier.";
              }

              if (::isatty(STDIN_FILENO) == 0) {
                  qWarning()
                    << "The standard input fd isn't a tty, terminal notifier is unavailable";
                  break;
              }

              if (::isatty(STDOUT_FILENO) == 0) {
                  qWarning()
                    << "The standard output fd isn't a tty, terminal notifier is unavailable";
                  break;
              }

              notifier = std::make_unique<TerminalNotifier>();
              break;
          }

          if (noDBus) {
              notifier.reset();
              notifier = std::make_unique<linglong::cli::DummyNotifier>();
          }

          if (!notifier) {
              qCritical() << "no available notifier, exit";
              QCoreApplication::exit(-1);
              return;
          }

          auto *cli = new linglong::cli::Cli(*printer,
                                             **ociRuntime,
                                             *containerBuidler,
                                             *pkgMan,
                                             *repo,
                                             std::move(notifier),
                                             QCoreApplication::instance());
          cli->setCliOptions(options);
          if (repo->needMigrate()) {
              QCoreApplication::exit(cli->migrate());
              return;
          }

          QMap<QString, std::function<int(Cli *)>> subcommandMap = {
              { "run", &Cli::run },
              { "exec", &Cli::exec },
              { "enter", &Cli::exec },
              { "ps", &Cli::ps },
              { "kill", &Cli::kill },
              { "install", &Cli::install },
              { "upgrade", &Cli::upgrade },
              { "search", &Cli::search },
              { "uninstall", &Cli::uninstall },
              { "list", &Cli::list },
              { "info", &Cli::info },
              { "content", &Cli::content },
              { "migrate", &Cli::migrate }
          };

          if (QObject::connect(QCoreApplication::instance(),
                               &QCoreApplication::aboutToQuit,
                               cli,
                               &Cli::cancelCurrentTask)
              == nullptr) {
              qCritical() << "failed to connect signal: aboutToQuit";
              QCoreApplication::exit(-1);
              return;
          }

          auto commands = commandParser.get_subcommands();

          auto ret =
            std::find_if(commands.begin(), commands.end(), [&subcommandMap](CLI::App *app) {
                auto name = app->get_name();
                return app->parsed()
                  && (name == "repo" || subcommandMap.contains(QString::fromStdString(name)));
            });

          auto name = (*ret)->get_name();
          QString subcommand = QString::fromStdString(name);
          if (ret == commands.end()) {
              qWarning() << "subcommand" << subcommand << "not found"
                         << "please see help for more information:";
              return;
          }

          // ll-cli repo need app to parse subcommand
          return QCoreApplication::exit(name == "repo" ? cli->repo(*ret)
                                                       : subcommandMap[subcommand](cli));
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
