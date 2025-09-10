/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "configure.h"
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
    std::vector<std::string> res;
    std::reverse_copy(argv + 1, argv + argc, std::back_inserter(res));
    if (std::find(res.rbegin(), res.rend(), "run") == res.rend()) {
        return res;
    }

    auto exec = std::find(res.rbegin(), res.rend(), "--exec");
    if (exec == res.rend()) {
        return res;
    }

    if ((exec + 1) == res.rend() || (exec + 2) != res.rend()) {
        *exec = "--";
        qDebug() << "replace `--exec` with `--`";
        return res;
    }

    wordexp_t words;
    auto _ = linglong::utils::finally::finally([&]() {
        wordfree(&words);
    });

    if (auto ret = wordexp((exec + 1)->c_str(), &words, 0); ret != 0) {
        qCritical() << "wordexp on" << (exec + 1)->c_str() << "failed with" << ret
                    << "transform old exec arguments failed.";
        return res;
    }

    auto it = res.erase(res.rend().base(), exec.base());
    res.emplace(it, "--");
    for (decltype(words.we_wordc) i = 0; i < words.we_wordc; ++i) {
        res.emplace(res.begin(), words.we_wordv[i]);
    }

    return res;
}

int lockCheck() noexcept
{
    std::error_code ec;
    constexpr auto lock = "/run/linglong/lock";
    auto fd = ::open(lock, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            return 0;
        }

        qCritical() << "failed to open lock" << lock << ::strerror(errno);
        return -1;
    }

    auto closeFd = linglong::utils::finally::finally([fd]() {
        ::close(fd);
    });

    struct flock lock_info{ .l_type = F_RDLCK,
                            .l_whence = SEEK_SET,
                            .l_start = 0,
                            .l_len = 0,
                            .l_pid = 0 };

    if (::fcntl(fd, F_GETLK, &lock_info) == -1) {
        qCritical() << "failed to get lock" << lock;
        return -1;
    }

    if (lock_info.l_type == F_UNLCK) {
        return 0;
    }

    return lock_info.l_pid;
}

// Validator for string inputs
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

// Function to add the run subcommand
void addRunCommand(CLI::App &commandParser, RunOptions &runOptions, const std::string &group)
{
    auto *cliRun =
      commandParser.add_subcommand("run", _("Run an application"))->group(group)->fallthrough();

    // add sub command run options
    cliRun->add_option("APP", runOptions.appid, _("Specify the application ID"))
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
      ->add_option("--file",
                   runOptions.filePaths,
                   _("Pass file to applications running in a sandbox"))
      ->type_name("FILE")
      ->expected(0, -1);
    cliRun
      ->add_option("--url", runOptions.fileUrls, _("Pass url to applications running in a sandbox"))
      ->type_name("URL")
      ->expected(0, -1);
    cliRun->add_option("--env", runOptions.envs, _("Set environment variables for the application"))
      ->type_name("ENV")
      ->expected(0, -1)
      ->check([](const std::string &env) -> std::string {
          if (env.find('=') == std::string::npos) {
              return std::string{ _(
                "Input parameter is invalid, please input valid parameter instead") };
          }

          return {};
      });
    cliRun
      ->add_option("--base", runOptions.base, _("Specify the base used by the application to run"))
      ->type_name("REF")
      ->check(validatorString);
    cliRun
      ->add_option("--runtime",
                   runOptions.runtime,
                   _("Specify the runtime used by the application to run"))
      ->type_name("REF")
      ->check(validatorString);
    cliRun->add_option("COMMAND", runOptions.commands, _("Run commands in a running sandbox"));
}

// Function to add the ps subcommand
void addPsCommand(CLI::App &commandParser, const std::string &group)
{
    commandParser.add_subcommand("ps", _("List running applications"))
      ->fallthrough()
      ->group(group)
      ->usage(_("Usage: ll-cli ps [OPTIONS]"));
}

// Function to add the exec subcommand
void addEnterCommand(CLI::App &commandParser, EnterOptions &enterOptions, const std::string &group)
{

    auto *cliEnter =
      commandParser
        .add_subcommand("enter", _("Enter the namespace where the application is running"))
        ->fallthrough()
        ->group(group);
    cliEnter
      ->add_option("INSTANCE",
                   enterOptions.instance,
                   _("Specify the application running instance(you can get it by ps command)"))
      ->required()
      ->check(validatorString);
    cliEnter
      ->add_option("--working-directory", enterOptions.workDir, _("Specify working directory"))
      ->type_name("PATH")
      ->check(CLI::ExistingDirectory);
    cliEnter->add_option("COMMAND", enterOptions.commands, _("Run commands in a running sandbox"));
}

// Function to add the kill subcommand
void addKillCommand(CLI::App &commandParser, KillOptions &killOptions, const std::string &group)
{
    auto *cliKill = commandParser.add_subcommand("kill", _("Stop running applications"))
                      ->group(group)
                      ->fallthrough();
    cliKill->usage(_("Usage: ll-cli kill [OPTIONS] APP"));
    cliKill
      ->add_option("-s,--signal",
                   killOptions.signal,
                   _("Specify the signal to send to the application"))
      ->default_val("SIGTERM");
    cliKill->add_option("APP", killOptions.appid, _("Specify the running application"))
      ->required()
      ->check(validatorString);
}

// Function to add the install subcommand
void addInstallCommand(CLI::App &commandParser,
                       InstallOptions &installOptions,
                       const std::string &group)
{
    auto *cliInstall =
      commandParser.add_subcommand("install", _("Installing an application or runtime"))
        ->group(group)
        ->fallthrough();
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
                   installOptions.appid,
                   _("Specify the application ID, and it can also be a .uab or .layer file"))
      ->required()
      ->check(validatorString);
    cliInstall->add_option("--module", installOptions.module, _("Install a specify module"))
      ->type_name("MODULE")
      ->check(validatorString);
    cliInstall->add_option("--repo", installOptions.repo, _("Install from a specific repo"))
      ->type_name("REPO")
      ->check(validatorString);
    cliInstall->add_flag("--force", installOptions.forceOpt, _("Force install the application"));
    cliInstall->add_flag("-y",
                         installOptions.confirmOpt,
                         _("Automatically answer yes to all questions"));
}

// Function to add the uninstall subcommand
void addUninstallCommand(CLI::App &commandParser,
                         UninstallOptions &uninstallOptions,
                         const std::string &group)
{
    auto *cliUninstall =
      commandParser.add_subcommand("uninstall", _("Uninstall the application or runtimes"))
        ->group(group)
        ->fallthrough();
    cliUninstall->usage(_("Usage: ll-cli uninstall [OPTIONS] APP"));
    cliUninstall->add_option("APP", uninstallOptions.appid, _("Specify the applications ID"))
      ->required()
      ->check(validatorString);
    cliUninstall->add_option("--module", uninstallOptions.module, _("Uninstall a specify module"))
      ->type_name("MODULE")
      ->check(validatorString);

    // below options are used for compatibility with old ll-cli
    const auto &pruneDescription = std::string{ _("Remove all unused modules") };
    [[maybe_unused]] auto *pruneFlag =
      cliUninstall->add_flag("--prune", pruneDescription)->group("");

    const auto &allDescription = std::string{ _("Uninstall all modules") };
    [[maybe_unused]] auto *allFlag = cliUninstall->add_flag("--all", allDescription)->group("");
}

// Function to add the upgrade subcommand
void addUpgradeCommand(CLI::App &commandParser,
                       UpgradeOptions &upgradeOptions,
                       const std::string &group)
{
    auto *cliUpgrade =
      commandParser.add_subcommand("upgrade", _("Upgrade the application or runtimes"))
        ->group(group)
        ->fallthrough();
    cliUpgrade->usage(_("Usage: ll-cli upgrade [OPTIONS] [APP]"));
    cliUpgrade
      ->add_option("APP",
                   upgradeOptions.appid,
                   _("Specify the application ID. If it not be specified, all "
                     "applications will be upgraded"))
      ->check(validatorString);
}

// Function to add the search subcommand
void addSearchCommand(CLI::App &commandParser,
                      SearchOptions &searchOptions,
                      const std::string &group)
{
    auto *cliSearch = commandParser
                        .add_subcommand("search",
                                        _("Search the applications/runtimes containing the "
                                          "specified text from the remote repository"))
                        ->fallthrough()
                        ->group(group);
    cliSearch->usage(_(R"(Usage: ll-cli search [OPTIONS] KEYWORDS

Example:
# find remotely application(s), base(s) or runtime(s) by keywords
ll-cli search org.deepin.demo
# find all of app of remote
ll-cli search .
# find all of base(s) of remote
ll-cli search . --type=base
# find all of runtime(s) of remote
ll-cli search . --type=runtime)"));
    cliSearch->add_option("KEYWORDS", searchOptions.appid, _("Specify the Keywords"))
      ->required()
      ->check(validatorString);
    cliSearch
      ->add_option(
        "--type",
        searchOptions.type,
        _(R"(Filter result with specify type. One of "runtime", "base", "app" or "all")"))
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliSearch->add_option("--repo", searchOptions.repo, _("Specify the repo"))
      ->type_name("REPO")
      ->check(validatorString);
    cliSearch->add_flag("--dev",
                        searchOptions.showDevel,
                        _("Include develop application in result"));
    cliSearch->add_flag("--show-all-version",
                        searchOptions.showAllVersion,
                        _("Show all versions of an application(s), base(s) or runtime(s)"));
}

// Function to add the list subcommand
void addListCommand(CLI::App &commandParser, ListOptions &listOptions, const std::string &group)
{
    auto *cliList =
      commandParser
        .add_subcommand("list", _("List installed application(s), base(s) or runtime(s)"))
        ->fallthrough()
        ->group(group);
    cliList->usage(_(R"(Usage: ll-cli list [OPTIONS]

Example:
# show installed application(s), base(s) or runtime(s)
ll-cli list
# show installed base(s)
ll-cli list --type=base
# show installed runtime(s)
ll-cli list --type=runtime
# show the latest version list of the currently installed application(s)
ll-cli list --upgradable
)"));
    cliList
      ->add_option(
        "--type",
        listOptions.type,
        _(R"(Filter result with specify type. One of "runtime", "base", "app" or "all")"))
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliList->add_flag("--upgradable",
                      listOptions.showUpgradeList,
                      _("Show the list of latest version of the currently installed "
                        "application(s), base(s) or runtime(s)"));
}

// Function to add the repo subcommand
void addRepoCommand(CLI::App &commandParser, RepoOptions &repoOptions, const std::string &group)
{
    auto *cliRepo =
      commandParser
        .add_subcommand("repo",
                        _("Display or modify information of the repository currently using"))
        ->group(group);
    cliRepo->usage(_("Usage: ll-cli repo SUBCOMMAND [OPTIONS]"));
    cliRepo->require_subcommand(1);

    // add repo sub command add
    auto *repoAdd = cliRepo->add_subcommand("add", _("Add a new repository"));
    repoAdd->usage(_("Usage: ll-cli repo add [OPTIONS] NAME URL"));
    repoAdd->add_option("NAME", repoOptions.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    repoAdd->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);
    repoAdd->add_option("--alias", repoOptions.repoAlias, _("Alias of the repo name"))
      ->type_name("ALIAS")
      ->check(validatorString);

    // add repo sub command modify
    auto *repoModify = cliRepo->add_subcommand("modify", _("Modify repository URL"))->group("");
    repoModify->add_option("--name", repoOptions.repoName, _("Specify the repo name"))
      ->type_name("REPO")
      ->check(validatorString);
    repoModify->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command remove
    auto *repoRemove = cliRepo->add_subcommand("remove", _("Remove a repository"));
    repoRemove->usage(_("Usage: ll-cli repo remove [OPTIONS] NAME"));
    repoRemove->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    // TODO: add --repo and --url options
    auto *repoUpdate = cliRepo->add_subcommand("update", _("Update the repository URL"));
    repoUpdate->usage(_("Usage: ll-cli repo update [OPTIONS] NAME URL"));
    repoUpdate->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    repoUpdate->add_option("URL", repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command set-default
    auto *repoSetDefault =
      cliRepo->add_subcommand("set-default", _("Set a default repository name"));
    repoSetDefault->usage(_("Usage: ll-cli repo set-default [OPTIONS] NAME"));
    repoSetDefault->add_option("Alias", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command show
    cliRepo->add_subcommand("show", _("Show repository information"))
      ->usage(_("Usage: ll-cli repo show [OPTIONS]"));

    // add repo sub command set-priority
    auto *repoSetPriority =
      cliRepo->add_subcommand("set-priority", _("Set the priority of the repo"));
    repoSetPriority->usage(_("Usage: ll-cli repo set-priority ALIAS PRIORITY"));
    repoSetPriority->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    repoSetPriority->add_option("PRIORITY", repoOptions.repoPriority, _("Priority of the repo"))
      ->required()
      ->check(validatorString);
    // add repo sub command enable mirror
    auto *repoEnableMirror =
      cliRepo->add_subcommand("enable-mirror", _("Enable mirror for the repo"));
    repoEnableMirror->usage(_("Usage: ll-cli repo enable-mirror [OPTIONS] ALIAS"));
    repoEnableMirror->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    // add repo sub command disable mirror
    auto *repoDisableMirror =
      cliRepo->add_subcommand("disable-mirror", _("Disable mirror for the repo"));
    repoDisableMirror->usage(_("Usage: ll-cli repo disable-mirror [OPTIONS] ALIAS"));
    repoDisableMirror->add_option("ALIAS", repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
}

// Function to add the info subcommand
void addInfoCommand(CLI::App &commandParser, InfoOptions &infoOptions, const std::string &group)
{
    auto *cliInfo =
      commandParser
        .add_subcommand("info", _("Display information about installed apps or runtimes"))
        ->fallthrough()
        ->group(group);
    cliInfo->usage(_("Usage: ll-cli info [OPTIONS] APP"));
    cliInfo
      ->add_option("APP",
                   infoOptions.appid,
                   _("Specify the application ID, and it can also be a .layer file"))
      ->required()
      ->check(validatorString);
}

// Function to add the content subcommand
void addContentCommand(CLI::App &commandParser,
                       ContentOptions &contentOptions,
                       const std::string &group)
{
    auto *cliContent =
      commandParser
        .add_subcommand("content", _("Display the exported files of installed application"))
        ->fallthrough()
        ->group(group);
    cliContent->usage(_("Usage: ll-cli content [OPTIONS] APP"));
    cliContent->add_option("APP", contentOptions.appid, _("Specify the installed application ID"))
      ->required()
      ->check(validatorString);
}

// Function to add the prune subcommand
void addPruneCommand(CLI::App &commandParser, const std::string &group)
{
    commandParser.add_subcommand("prune", _("Remove the unused base or runtime"))
      ->group(group)
      ->usage(_("Usage: ll-cli prune [OPTIONS]"));
}

// Function to add the inspect subcommand
void addInspectCommand(CLI::App &commandParser,
                       InspectOptions &inspectOptions,
                       const std::string &group)
{
    auto *cliInspect =
      commandParser
        .add_subcommand("inspect", _("Display the information of installed application"))
        ->group(group)
        ->usage(_("Usage: ll-cli inspect [OPTIONS]"));
    cliInspect->footer("This subcommand is for internal use only currently");
    cliInspect->add_option("-p,--pid", inspectOptions.pid, _("Specify the process id"))
      ->check([](const std::string &input) -> std::string {
          if (input.empty()) {
              return _("Input parameter is empty, please input valid parameter instead");
          }

          try {
              auto pid = std::stoull(input);
              if (pid <= 0) {
                  return _("Invalid process id");
              }
          } catch (std::exception &e) {
              return _("Invalid pid format");
          }

          return {};
      });
}

// Function to add the dir subcommand
void addDirCommand(CLI::App &commandParser, DirOptions &dirOptions, const std::string &group)
{
    auto cliLayerDir =
      commandParser.add_subcommand("dir", "Get the layer directory of app(base or runtime)")
        ->group(group);
    cliLayerDir->footer("This subcommand is for internal use only currently");
    cliLayerDir
      ->add_option("APP", dirOptions.appid, _("Specify the installed app(base or runtime)"))
      ->required()
      ->check(validatorString);
    cliLayerDir->add_option("--module", dirOptions.module, _("Specify a module"))
      ->type_name("MODULE")
      ->check(validatorString);
}

} // namespace

using namespace linglong::utils::global;

// 初始化仓库
linglong::utils::error::Result<linglong::repo::OSTreeRepo *> initOSTreeRepo()
{
    LINGLONG_TRACE("initOSTreeRepo");
    // load repo config
    auto repoConfig = linglong::repo::loadConfig(
      { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
    if (!repoConfig) {
        return LINGLONG_ERR("load repo config failed", repoConfig);
    }

    // get default repo
    auto defaultRepo = linglong::repo::getDefaultRepo(*repoConfig);
    auto clientFactory = new linglong::repo::ClientFactory(std::move(defaultRepo.url));
    clientFactory->setParent(QCoreApplication::instance());

    // check repo root
    auto repoRoot = QDir(LINGLONG_ROOT);
    if (!repoRoot.exists()) {
        return LINGLONG_ERR("repo root doesn't exist" + repoRoot.absolutePath());
    }

    // create repo
    auto repo = new linglong::repo::OSTreeRepo(repoRoot, std::move(*repoConfig), *clientFactory);
    repo->setParent(QCoreApplication::instance());
    return repo;
}

int runCliApplication(int argc, char **mainArgv)
{
    CLI::App commandParser{ _(
      "linyaps CLI\n"
      "A CLI program to run application and manage application and runtime\n") };
    auto argv = commandParser.ensure_utf8(mainArgv);
    if (argc == 1) {
        std::cout << commandParser.help() << std::endl;
        return 0;
    }

    commandParser.get_help_ptr()->description(_("Print this help message and exit"));
    commandParser.set_help_all_flag("--help-all", _("Expand all help"));
    commandParser.usage(_("Usage: ll-cli [OPTIONS] [SUBCOMMAND]"));
    commandParser.footer(_(R"(If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues)"));

    // group empty will hide command
    constexpr auto CliHiddenGroup = "";

    // version flag
    const auto &versionDescription = std::string{ _("Show version") };
    auto *versionFlag = commandParser.add_flag("--version", versionDescription);

    // no-dbus flag
    const auto &noDBusDescription = std::string{ _(
      "Use peer to peer DBus, this is used only in case that DBus daemon is not available") };
    auto *noDBusFlag =
      commandParser.add_flag("--no-dbus", noDBusDescription)->group(CliHiddenGroup);

    // json flag
    const auto &jsonDescription = std::string{ _("Use json format to output result") };
    auto *jsonFlag = commandParser.add_flag("--json", jsonDescription);

    // verbose flag
    GlobalOptions globalOptions{ .verbose = false };
    commandParser.add_flag("-v,--verbose",
                           globalOptions.verbose,
                           _("Show debug info (verbose logs)"));

    // subcommand options
    RunOptions runOptions{};
    EnterOptions enterOptions{};
    KillOptions killOptions{};
    InstallOptions installOptions{};
    UpgradeOptions upgradeOptions{};
    SearchOptions searchOptions{};
    UninstallOptions uninstallOptions{};
    ListOptions listOptions{};
    InfoOptions infoOptions{};
    ContentOptions contentOptions{};
    RepoOptions repoOptions{};
    InspectOptions inspectOptions{};
    DirOptions dirOptions{};

    // groups for subcommands
    auto *CliBuildInGroup = _("Managing installed applications and runtimes");
    auto *CliAppManagingGroup = _("Managing running applications");
    auto *CliSearchGroup = _("Finding applications and runtimes");
    auto *CliRepoGroup = _("Managing remote repositories");

    // add all subcommands using the new functions
    addRunCommand(commandParser, runOptions, CliAppManagingGroup);
    addPsCommand(commandParser, CliAppManagingGroup);
    addEnterCommand(commandParser, enterOptions, CliAppManagingGroup);
    addKillCommand(commandParser, killOptions, CliAppManagingGroup);
    addInstallCommand(commandParser, installOptions, CliBuildInGroup);
    addUninstallCommand(commandParser, uninstallOptions, CliBuildInGroup);
    addUpgradeCommand(commandParser, upgradeOptions, CliBuildInGroup);
    addSearchCommand(commandParser, searchOptions, CliSearchGroup);
    addListCommand(commandParser, listOptions, CliBuildInGroup);
    addRepoCommand(commandParser, repoOptions, CliRepoGroup);
    addInfoCommand(commandParser, infoOptions, CliBuildInGroup);
    addContentCommand(commandParser, contentOptions, CliBuildInGroup);
    addPruneCommand(commandParser, CliAppManagingGroup);
    addInspectCommand(commandParser, inspectOptions, CliHiddenGroup);
    addDirCommand(commandParser, dirOptions, CliHiddenGroup);

    auto res = transformOldExec(argc, argv);
    CLI11_PARSE(commandParser, std::move(res));

    // print version if --version flag is set
    if (*versionFlag) {
        if (*jsonFlag) {
            std::cout << nlohmann::json{ { "version", LINGLONG_VERSION } } << std::endl;
        } else {
            std::cout << _("linyaps CLI version ") << LINGLONG_VERSION << std::endl;
        }
        return 0;
    }

    // check lock
    while (true) {
        auto lockOwner = lockCheck();
        if (lockOwner == -1) {
            qCritical() << "lock check failed";
            return -1;
        }

        if (lockOwner > 0) {
            qInfo() << "\r\33[K"
                    << "\033[?25l"
                    << "repository is being operated by another process, waiting for" << lockOwner
                    << "\033[?25h";
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);
            continue;
        }

        break;
    }

    // connect to package manager
    auto pkgManConn = QDBusConnection::systemBus();
    auto *pkgMan =
      new linglong::api::dbus::v1::PackageManager("org.deepin.linglong.PackageManager1",
                                                  "/org/deepin/linglong/PackageManager1",
                                                  pkgManConn,
                                                  QCoreApplication::instance());
    // if --no-dbus flag is set, start package manager in sudo mode
    if (!noDBusFlag) {
        if (getuid() != 0) {
            qCritical() << "--no-dbus should only be used by root user.";
            return -1;
        }

        qInfo() << "some subcommands will failed in --no-dbus mode.";
        const auto pkgManAddress = QString("unix:path=/tmp/linglong-package-manager.socket");
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
            qCritical() << "Failed to connect to ll-package-manager:" << pkgManConn.lastError();
            return -1;
        }

        pkgMan = new linglong::api::dbus::v1::PackageManager("",
                                                             "/org/deepin/linglong/PackageManager1",
                                                             pkgManConn,
                                                             QCoreApplication::instance());
    } else {
        // ping package manager to make it initialize system linglong repository
        auto peer = linglong::api::dbus::v1::DBusPeer("org.deepin.linglong.PackageManager1",
                                                      "/org/deepin/linglong/PackageManager1",
                                                      pkgManConn);
        auto reply = peer.Ping();
        reply.waitForFinished();
        if (!reply.isValid()) {
            qCritical() << "Failed to activate org.deepin.linglong.PackageManager1"
                        << reply.error();
            return -1;
        }
    }

    // create printer
    std::unique_ptr<Printer> printer;
    if (*jsonFlag) {
        printer = std::make_unique<JSONPrinter>();
    } else {
        printer = std::make_unique<CLIPrinter>();
    }

    // get oci runtime
    auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
    if (ociRuntimeCLI.isEmpty()) {
        ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
    }

    // check oci runtime
    auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
    if (path.isEmpty()) {
        qCritical() << ociRuntimeCLI << "not found";
        return -1;
    }

    // create oci runtime
    auto ociRuntime = ocppi::cli::crun::Crun::New(path.toStdString());
    if (!ociRuntime) {
        std::rethrow_exception(ociRuntime.error());
    }

    // create container builder
    auto *containerBuilder = new linglong::runtime::ContainerBuilder(**ociRuntime);
    containerBuilder->setParent(QCoreApplication::instance());

    // create notifier
    std::unique_ptr<InteractiveNotifier> notifier{ nullptr };

    // if ll-cli is running in tty, should use terminalNotifier.
    if (::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0) {
        notifier = std::make_unique<TerminalNotifier>();
    } else {
        try {
            notifier = std::make_unique<DBusNotifier>();
        } catch (std::runtime_error &err) {
            qInfo() << "initialize DBus notifier failed:" << err.what()
                    << "try to fallback to terminal notifier.";
        }
    }

    if (!notifier) {
        qInfo() << "Using DummyNotifier, expected interactions and prompts will not be "
                   "displayed.";
        notifier = std::make_unique<linglong::cli::DummyNotifier>();
    }
    auto repo = initOSTreeRepo();
    if (!repo.has_value()) {
        qCritical() << "initOSTreeRepo failed" << repo.error();
        return -1;
    }
    // create cli
    auto *cli = new linglong::cli::Cli(*printer,
                                       **ociRuntime,
                                       *containerBuilder,
                                       *pkgMan,
                                       **repo,
                                       std::move(notifier),
                                       QCoreApplication::instance());
    cli->setGlobalOptions(std::move(globalOptions));

    // connect signal
    if (QObject::connect(QCoreApplication::instance(),
                         &QCoreApplication::aboutToQuit,
                         cli,
                         &Cli::cancelCurrentTask)
        == nullptr) {
        qCritical() << "failed to connect signal: aboutToQuit";
        return -1;
    }

    // get subcommands
    const auto &commands = commandParser.get_subcommands();
    auto ret = std::find_if(commands.begin(), commands.end(), [](CLI::App *app) {
        return app->parsed();
    });

    // if no subcommand is set, print help
    if (ret == commands.end()) {
        std::cout << commandParser.help("", CLI::AppFormatMode::All);
        return -1;
    }

    // get subcommand name
    const auto &name = (*ret)->get_name();
    int result = -1;
    // call corresponding function according to subcommand name and pass corresponding options
    if (name == "run") {
        result = cli->run(runOptions);
    } else if (name == "enter") {
        result = cli->enter(enterOptions);
    } else if (name == "ps") {
        result = cli->ps();
    } else if (name == "kill") {
        result = cli->kill(killOptions);
    } else if (name == "install") {
        result = cli->install(installOptions);
    } else if (name == "upgrade") {
        result = cli->upgrade(upgradeOptions);
    } else if (name == "search") {
        result = cli->search(searchOptions);
    } else if (name == "uninstall") {
        result = cli->uninstall(uninstallOptions);
    } else if (name == "list") {
        result = cli->list(listOptions);
    } else if (name == "info") {
        result = cli->info(infoOptions);
    } else if (name == "content") {
        result = cli->content(contentOptions);
    } else if (name == "prune") {
        result = cli->prune();
    } else if (name == "inspect") {
        result = cli->inspect(inspectOptions);
    } else if (name == "repo") {
        result = cli->repo(*ret, repoOptions);
    } else if (name == "dir") {
        result = cli->dir(dirOptions);
    } else {
        // if subcommand name is not found, print help
        std::cout << commandParser.help("", CLI::AppFormatMode::All);
        return -1;
    }
    // return result
    return result;
}

int main(int argc, char **argv)
{
    // bind text domain
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    // text domain
    textdomain(PACKAGE_LOCALE_DOMAIN);

    QCoreApplication app(argc, argv);
    // application initialize
    applicationInitialize();
    initLinyapsLogSystem(argv[0]);

    // invoke method
    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [argc, argv]() {
          QCoreApplication::exit(runCliApplication(argc, argv));
      },
      Qt::QueuedConnection);
    // assert
    Q_ASSERT(ret);

    // exec
    return QCoreApplication::exec();
}
