/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/cli_printer.h"
#include "linglong/cli/dbus_notifier.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/json_printer.h"
#include "linglong/cli/terminal_notifier.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/global/initialize.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <CLI/CLI.hpp>
#include <sys/file.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <thread>

#include <fcntl.h>
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

void ensureDirectory(const std::filesystem::path &dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        if (ec) {
            qCritical() << QString{ "get status of" } << dir.c_str()
                        << "failed:" << ec.message().c_str();
            std::abort();
        }

        if (!std::filesystem::create_directory(dir, ec)) {
            qCritical() << "failed to create directory:" << ec.message().c_str();
            QCoreApplication::exit(ec.value());
            std::abort();
        }
    }
}

int lockCheck() noexcept
{
    std::error_code ec;
    constexpr auto lock = "/run/linglong/lock";
    if (!std::filesystem::exists(lock, ec)) {
        if (ec) {
            qCritical() << "failed to get status of" << lock;
            return -1;
        }

        return 0;
    }

    auto fd = ::open(lock, O_RDONLY);
    if (fd == -1) {
        qCritical() << "failed to open lock" << lock;
        return -1;
    }
    auto closeFd = linglong::utils::finally::finally([fd]() {
        ::close(fd);
    });

    struct flock lock_info
    {
        .l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0, .l_pid = 0
    };

    if (::fcntl(fd, F_GETLK, &lock_info) == -1) {
        qCritical() << "failed to get lock" << lock;
        return -1;
    }

    if (lock_info.l_type == F_UNLCK) {
        return 0;
    }

    return lock_info.l_pid;
}

} // namespace

using namespace linglong::utils::global;

int main(int argc, char **argv)
{
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);
    QCoreApplication app(argc, argv);

    applicationInitialize();

    bool versionFlag = false, jsonFlag = false, noDBus = false;

    CLI::App commandParser{ _(
      "linyaps CLI\n"
      "A CLI program to run application and manage application and runtime\n") };
    commandParser.get_help_ptr()->description(_("Print this help message and exit"));
    commandParser.set_help_all_flag("--help-all", _("Expand all help"));
    commandParser.usage(_("Usage: [OPTIONS] [SUBCOMMAND]"));
    commandParser.footer(_(R"(If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues)"));

    // group empty will hide command
    std::string CliHiddenGroup = "";

    // add flags
    commandParser.add_flag("--version", versionFlag, _("Show version"));
    commandParser
      .add_flag(
        "--no-dbus",
        noDBus,
        _("Use peer to peer DBus, this is used only in case that DBus daemon is not available"))
      ->group(CliHiddenGroup);
    commandParser.add_flag("--json", jsonFlag, _(R"(Use json format to output result)"));

    CLI::Validator validatorString{
        [](const std::string &parameter) {
            if (parameter.empty()) {
                return std::string{ _(
                  "Input parameter is empty, please input valid parameter instead") };
            }
            return std::string();
        },
        ""
    };

    CliOptions options = CliOptions{ .filePath = "",
                                     .fileUrl = "",
                                     .workDir = "",
                                     .appid = "",
                                     .instance = "",
                                     .module = "",
                                     .type = "app",
                                     .repoName = "",
                                     .repoUrl = "",
                                     .commands = {},
                                     .showDevel = false,
                                     .showUpgradeList = false,
                                     .forceOpt = false,
                                     .confirmOpt = false };

    // groups
    std::string CliBuildInGroup = _("Managing installed applications and runtimes");
    std::string CliAppManagingGroup = _("Managing running applications");
    std::string CliSearchGroup = _("Finding applications and runtimes");
    std::string CliRepoGroup = _("Managing remote repositories");

    // add sub command run
    std::vector<std::string> oldCommands;
    auto cliRun =
      commandParser.add_subcommand("run", _("Run an application"))->group(CliAppManagingGroup);

    // add sub command run options
    cliRun->add_option("APP", options.appid, _("Specify the application ID"))
      ->required()
      ->check(validatorString);
    cliRun->usage(_(R"(Usage: ll-cli run [OPTIONS] APP [COMMAND...]

Example:
# run application by appid
ll-cli run org.deepin.demo
# execute commands in the container rather than running the application
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script)"));
    cliRun
      ->add_option("--file", options.filePath, _("Pass file to applications running in a sandbox"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    cliRun->add_option("--url", options.fileUrl, _("Pass url to applications running in a sandbox"))
      ->type_name("URL");
    cliRun->add_option("COMMAND", options.commands, _("Run commands in a running sandbox"));

    // add sub command ps
    commandParser.add_subcommand("ps", _("List running applications"))->group(CliAppManagingGroup);

    // add sub command exec
    auto cliExec =
      commandParser.add_subcommand("exec", _("Execute commands in the currently running sandbox"))
        ->group(CliHiddenGroup);
    cliExec
      ->add_option("INSTANCE",
                   options.instance,
                   _("Specify the application running instance(you can get it by ps command)"))
      ->required()
      ->check(validatorString);
    cliExec->add_option("--working-directory", options.workDir, _("Specify working directory"))
      ->type_name("PATH")
      ->check(CLI::ExistingDirectory);
    cliExec->add_option("COMMAND", options.commands, _("Run commands in a running sandbox"));

    // add sub command enter
    auto cliEnter =
      commandParser
        .add_subcommand("enter", _("Enter the namespace where the application is running"))
        ->group(CliAppManagingGroup);
    cliEnter
      ->add_option("INSTANCE",
                   options.instance,
                   _("Specify the application running instance(you can get it by ps command)"))
      ->required();
    cliEnter->add_option("--working-directory", options.workDir, _("Specify working directory"))
      ->type_name("PATH")
      ->check(CLI::ExistingDirectory);
    cliEnter->add_option("COMMAND", options.commands, _("Run commands in a running sandbox"));

    // add sub command kill
    auto cliKill = commandParser.add_subcommand("kill", _("Stop running applications"))
                     ->group(CliAppManagingGroup);
    cliKill
      ->add_option("INSTANCE",
                   options.instance,
                   _("Specify the application running instance(you can get it by ps command)"))
      ->required()
      ->check(validatorString);

    // add sub command install
    auto cliInstall =
      commandParser.add_subcommand("install", _("Installing an application or runtime"))
        ->group(CliBuildInGroup);
    cliInstall->usage(_(R"(Usage: ll-cli install [OPTIONS] APP

Example:
# install application by appid
ll-cli install org.deepin.demo
# install application by linyaps layer
ll-cli install demo_0.0.0.1_x86_64_binary.layer
# install application by linyaps uab
ll-cli install demo_x86_64_0.0.0.1_main.uab
# install specified module of the appid
ll-cli install org.deepin.demo --module=binary
# install specified version of the appid
ll-cli install org.deepin.demo/0.0.0.1
# install application by detailed reference
ll-cli install stable:org.deepin.demo/0.0.0.1/x86_64
    )"));
    cliInstall
      ->add_option("APP",
                   options.appid,
                   _("Specify the application ID, and it can also be a .uab or .layer file"))
      ->required()
      ->check(validatorString);
    cliInstall->add_option("--module", options.module, _("Install a specify module"))
      ->type_name("MODULE")
      ->check(validatorString);
    cliInstall->add_flag("--force", options.forceOpt, _("Force install the application"));
    cliInstall->add_flag("-y", options.confirmOpt, _("Automatically answer yes to all questions"));

    // add sub command uninstall
    auto cliUninstall =
      commandParser.add_subcommand("uninstall", _("Uninstall the application or runtimes"))
        ->group(CliBuildInGroup);
    cliUninstall->add_option("APP", options.appid, _("Specify the applications ID"))
      ->required()
      ->check(validatorString);
    cliUninstall->add_option("--module", options.module, _("Uninstall a specify module"))
      ->type_name("MODULE")
      ->check(validatorString);

    // add sub command upgrade
    auto cliUpgrade =
      commandParser.add_subcommand("upgrade", _("Upgrade the application or runtimes"))
        ->group(CliBuildInGroup);
    cliUpgrade
      ->add_option(
        "APP",
        options.appid,
        _("Specify the application ID.If it not be specified, all applications will be upgraded"))
      ->check(validatorString);

    // add sub command search
    auto cliSearch = commandParser
                       .add_subcommand("search",
                                       _("Search the applications/runtimes containing the "
                                         "specified text from the remote repository"))
                       ->group(CliSearchGroup);
    cliSearch->usage(_(R"(Usage: ll-cli search [OPTIONS] KEYWORDS

Example:
# find remotely app by name
ll-cli search org.deepin.demo
# find remotely runtime by name
ll-cli search org.deepin.base --type=runtime
# find all off app of remote
ll-cli search .
# find all off runtime of remote
ll-cli search . --type=runtime)"));
    cliSearch->add_option("KEYWORDS", options.appid, _("Specify the Keywords"))
      ->required()
      ->check(validatorString);
    cliSearch
      ->add_option("--type",
                   options.type,
                   _(R"(Filter result with specify type. One of "runtime", "app" or "all")"))
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliSearch->add_flag("--dev", options.showDevel, _("include develop application in result"));

    // add sub command list
    auto cliList =
      commandParser.add_subcommand("list", _("List installed applications or runtimes"))
        ->group(CliBuildInGroup);
    cliList->usage(_(R"(Usage: ll-cli list [OPTIONS]

Example:
# show installed application(s)
ll-cli list
# show installed runtime(s)
ll-cli list --type=runtime
# show the latest version list of the currently installed application(s)
ll-cli list --upgradable
)"));
    cliList
      ->add_option("--type",
                   options.type,
                   _(R"(Filter result with specify type. One of "runtime", "app" or "all")"))
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliList->add_flag("--upgradable",
                      options.showUpgradeList,
                      _("Show the list of latest version of the currently installed applications, "
                        "it only works for app"));

    // add sub command repo
    auto cliRepo =
      commandParser
        .add_subcommand("repo",
                        _("Display or modify information of the repository currently using"))
        ->group(CliRepoGroup);
    cliRepo->require_subcommand(1);

    // add repo sub command add
    auto repoAdd = cliRepo->add_subcommand("add", _("Add a new repository"));
    repoAdd->add_option("NAME", options.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    repoAdd->add_option("URL", options.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command modify
    auto repoModify = cliRepo->add_subcommand("modify", _("Modify repository URL"));
    repoModify->add_option("--name", options.repoName, _("Specify the repo name"))
      ->type_name("REPO")
      ->check(validatorString);
    repoModify->add_option("URL", options.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command remove
    auto repoRemove = cliRepo->add_subcommand("remove", _("Remove a repository"));
    repoRemove->add_option("NAME", options.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto repoUpdate = cliRepo->add_subcommand("update", _("Update to a new repository"));
    repoUpdate->add_option("NAME", options.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    repoUpdate->add_option("URL", options.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command set-default
    auto repoSetDefault = cliRepo->add_subcommand("set-default", _("Set default repository name"));
    repoSetDefault->add_option("NAME", options.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command show
    cliRepo->add_subcommand("show", _("Show repository"));

    // add sub command info
    auto cliInfo =
      commandParser
        .add_subcommand("info", _("Display information about installed apps or runtimes"))
        ->group(CliBuildInGroup);
    cliInfo
      ->add_option("APP",
                   options.appid,
                   _("Specify the application ID, and it can also be a .layer file"))
      ->required()
      ->check(validatorString);

    // add sub command content
    auto cliContent =
      commandParser
        .add_subcommand("content", _("Display the exported files of installed application"))
        ->group(CliBuildInGroup);
    cliContent->add_option("APP", options.appid, _("Specify the installed application ID"))
      ->required()
      ->check(validatorString);

    // add sub command migrate
    commandParser.add_subcommand("migrate", _("Migrate repository data"))->group(CliHiddenGroup);

    // add sub command prune
    commandParser.add_subcommand("prune", _("Remove the unused base or runtime"))
      ->group(CliAppManagingGroup);

    auto res = transformOldExec(argc, argv);

    std::reverse(res.begin(), res.end());

    CLI11_PARSE(commandParser, res);

    if (versionFlag) {
        if (jsonFlag) {
            std::cout << nlohmann::json{ { "version", LINGLONG_VERSION } } << std::endl;
        } else {
            std::cout << _("linyaps CLI version ") << LINGLONG_VERSION << std::endl;
        }

        return 0;
    }

    if (options.commands.empty() && !oldCommands.empty()) {
        options.commands = oldCommands;
    }

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [&argc, &argv, &commandParser, &noDBus, &jsonFlag, &options]() {
          auto repoRoot = QDir(LINGLONG_ROOT);
          if (!repoRoot.exists()) {
              qCritical() << "underlying repository doesn't exist:" << repoRoot.absolutePath();
              QCoreApplication::exit(-1);
              return;
          }

          auto userContainerDir =
            std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());
          ensureDirectory(userContainerDir);

          while (true) {
              auto lockOwner = lockCheck();
              if (lockOwner == -1) {
                  qCritical() << "lock check failed";
                  QCoreApplication::exit(-1);
                  return;
              }

              if (lockOwner > 0) {
                  qInfo() << "\r\33[K"
                          << "\033[?25l"
                          << "repository is being operated by another process, waiting for"
                          << lockOwner << "\033[?25h";
                  using namespace std::chrono_literals;
                  std::this_thread::sleep_for(1s);
                  continue;
              }

              break;
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
              printer = std::make_unique<CLIPrinter>();
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

          // if ll-cli is running in tty, should use terminalNotifier.
          if (::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0) {
              notifier = std::make_unique<TerminalNotifier>();
          } else {
              try {
                  notifier = std::make_unique<DBusNotifier>();
              } catch (std::runtime_error &err) {
                  qWarning() << "initialize DBus notifier error:" << err.what()
                             << "try to fallback to terminal notifier.";
              }
          }

          if (!notifier) {
              qInfo() << "Using DummyNotifier, expected interactions and prompts will not be displayed.";
              notifier = std::make_unique<linglong::cli::DummyNotifier>();
          }

          // Note: Make sure migrateion is completed before other operations if need migrate here.
          auto migrateOption = commandParser.get_subcommand("migrate");
          if (repo->needMigrate() && !migrateOption->parsed()) {
              notifier->notify(linglong::api::types::v1::InteractionRequest{
                .summary = "The old data is found locally and needs to be migrated. Please run "
                           "'ll-cli migrate' and wait for the migration to complete." });
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
              { "migrate", &Cli::migrate },
              { "prune", &Cli::prune }
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

          if (ret == commands.end()) {
              std::cout << commandParser.help("", CLI::AppFormatMode::All);
              return QCoreApplication::exit(-1);
          }

          auto name = (*ret)->get_name();

          // ll-cli repo need app to parse subcommand
          return QCoreApplication::exit(
            name == "repo" ? cli->repo(*ret) : subcommandMap[QString::fromStdString(name)](cli));
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
