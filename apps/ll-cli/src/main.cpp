/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "configure.h"
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
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
#include "linglong/utils/log/log.h"
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

#include <cstdio>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <wordexp.h>

#include <nlohmann/json.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <tuple>
#include <unordered_set>
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
      // vector parameter allow extra args by default, but we don't want it
      ->allow_extra_args(false)
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
    cliRun
      ->add_option("--extensions",
                   runOptions.extensions,
                   _("Specify extension(s) used by the application to run"))
      ->type_name("REF")
      ->delimiter(',')          // 支持以逗号分隔
      ->allow_extra_args(false) // 避免吞掉后面的参数
      ->check(validatorString);
    cliRun
      ->add_flag("--privileged", runOptions.privileged, _("Run the application in privileged mode"))
      ->group("");
    cliRun->add_option("--caps-add", runOptions.capsAdd, _("Add capabilities to the application"))
      ->delimiter(',')
      ->allow_extra_args(false)
      ->group("");
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
        .add_subcommand("inspect",
                        _("Display the inspect information of the installed application"))
        ->group(group)
        ->usage(_("Usage: ll-cli inspect SUBCOMMAND [OPTIONS]"));

    cliInspect->require_subcommand(1);

    auto *cliInspectDir = cliInspect->add_subcommand(
      "dir",
      _("Display the data(bundle) directory of the installed(running) application"));
    cliInspectDir->usage(_("Usage: ll-cli inspect dir [OPTIONS] APP"));
    cliInspectDir
      ->add_option("APP",
                   inspectOptions.appid,
                   _("Specify the application ID, and it can also be reference"))
      ->required()
      ->check(validatorString);
    cliInspectDir
      ->add_option("-t, --type",
                   inspectOptions.dirType,
                   _("Specify the directory type (layer or bundle),the default is layer"))
      ->type_name("TYPE")
      ->capture_default_str()
      ->check(validatorString);
    cliInspectDir
      ->add_option("-m, --module",
                   inspectOptions.module,
                   _("Specify the module type (binary or develop). Only works when type is layer"))
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

    // check repo root
    auto repoRoot = QDir(LINGLONG_ROOT);
    if (!repoRoot.exists()) {
        return LINGLONG_ERR("repo root doesn't exist" + repoRoot.absolutePath());
    }

    // create repo
    auto repo = new linglong::repo::OSTreeRepo(repoRoot, std::move(*repoConfig));
    repo->setParent(QCoreApplication::instance());
    return repo;
}


// ===== begin: ll-cli config helpers =====
using json = nlohmann::json;

static std::string configUsageLines()
{
    return std::string{
        _("  ll-cli config set-extensions [--global | --app <appid> | --base <baseid>] ext1,ext2\n"
           "  ll-cli config add-extensions [--global | --app <appid> | --base <baseid>] ext1,ext2\n"
           "  ll-cli config set-env        [--global | --app <appid> | --base <baseid>] KEY=VAL [KEY=VAL ...]\n"
           "  ll-cli config unset-env      [--global | --app <appid> | --base <baseid>] KEY [KEY ...]\n"
           "  ll-cli config add-fs         [--global | --app <appid> | --base <baseid>] --host PATH --target PATH "
           "[--mode ro|rw] [--persist]\n"
           "  ll-cli config rm-fs          [--global | --app <appid> | --base <baseid>] (--target PATH | --index N)\n"
           "  ll-cli config add-fs-allow   [--global | --app <appid> | --base <baseid>] --host PATH --target PATH "
           "[--mode ro|rw] [--persist]\n"
           "  ll-cli config rm-fs-allow    [--global | --app <appid> | --base <baseid>] (--target PATH | --index N)\n"
           "  ll-cli config clear-fs-allow [--global | --app <appid> | --base <baseid>]\n"
           "  ll-cli config set-command    [--global | --app <appid> | --base <baseid>] <cmd> [--entrypoint P] [--cwd D] "
           "[--args-prefix \"...\"] [--args-suffix \"...\"] [KEY=VAL ...]\n"
           "  ll-cli config unset-command  [--global | --app <appid> | --base <baseid>] <cmd>\n") };
}

static std::string configShortHelp()
{
    return std::string{ _("Configuration commands:\n"
                          "  config                      Manage ll-cli configuration (see `ll-cli config --help`)\n") };
}

static std::string configFooterMessage()
{
    return std::string{ _("If you found any problems during use,\n"
                          "You can report bugs to the linyaps team under this project: "
                          "https://github.com/OpenAtom-Linyaps/linyaps/issues") };
}

[[maybe_unused]] static void printConfigUsage(FILE *stream = stderr)
{
    auto usageLines = configUsageLines();
    std::fprintf(stream, "%s\n%s", _("Usage:"), usageLines.c_str());
}

enum class Scope { Global, App, Base };

static std::filesystem::path getBaseConfigDir()
{
    const char *xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        return std::filesystem::path(xdg) / "linglong";
    }
    const char *home = ::getenv("HOME");
    if (home && home[0]) {
        return std::filesystem::path(home) / ".config" / "linglong";
    }
    return {};
}

static std::filesystem::path getSystemConfigDir()
{
    return std::filesystem::path(LINGLONG_DATA_DIR) / "config";
}

static std::filesystem::path buildConfigPath(const std::filesystem::path &base,
                                             Scope scope,
                                             const std::string &appId,
                                             const std::string &baseId)
{
    if (base.empty()) {
        return {};
    }
    switch (scope) {
    case Scope::Global:
        return base / "config.json";
    case Scope::App:
        return base / "apps" / appId / "config.json";
    case Scope::Base:
        return base / "base" / baseId / "config.json";
    }
    return {};
}

static std::filesystem::path getConfigPath(Scope scope,
                                           const std::string &appId,
                                           const std::string &baseId)
{
    return buildConfigPath(getBaseConfigDir(), scope, appId, baseId);
}

static std::vector<std::filesystem::path> getConfigSearchPaths(Scope scope,
                                                               const std::string &appId,
                                                               const std::string &baseId)
{
    std::vector<std::filesystem::path> paths;
    std::unordered_set<std::string> seen;
    auto addPath = [&](const std::filesystem::path &candidate) {
        if (candidate.empty()) {
            return;
        }
        auto normalized = candidate.lexically_normal();
        auto key = normalized.string();
        if (!key.empty() && seen.insert(key).second) {
            paths.emplace_back(std::move(normalized));
        }
    };

    addPath(buildConfigPath(getBaseConfigDir(), scope, appId, baseId));
    addPath(buildConfigPath(getSystemConfigDir(), scope, appId, baseId));

    return paths;
}

static bool ensureParentDir(const std::filesystem::path &p)
{
    std::error_code ec;
    auto parent = p.parent_path();
    if (parent.empty()) {
        return true;
    }
    return std::filesystem::create_directories(parent, ec) || std::filesystem::exists(parent);
}

static std::optional<json> readJsonIfExists(const std::filesystem::path &p, bool *existed = nullptr)
{
    try {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) {
            if (existed) {
                *existed = false;
            }
            return json::object();
        }
        std::ifstream in(p);
        if (!in.is_open()) {
            return std::nullopt;
        }
        if (existed) {
            *existed = true;
        }
        json j;
        in >> j;
        return j.is_null() ? json::object() : j;
    } catch (...) {
        return std::nullopt;
    }
}

static bool writeJsonAtomic(const std::filesystem::path &p, const json &j)
{
    try {
        if (!ensureParentDir(p)) {
            return false;
        }
        auto tmp = p;
        tmp += ".tmp";
        {
            std::ofstream out(tmp);
            if (!out.is_open()) {
                return false;
            }
            out << j.dump(2) << "\n";
        }
        std::error_code ec;
        std::filesystem::rename(tmp, p, ec);
        return !ec;
    } catch (...) {
        return false;
    }
}

static std::vector<std::string> splitCsv(const std::string &s)
{
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) {
            out.push_back(tok);
        }
    }
    return out;
}

static std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return "";
    }
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static void jsonSetExtensions(json &root, const std::vector<std::string> &exts, bool overwrite)
{
    auto &arr = root["extensions"];
    if (!arr.is_array() || overwrite) {
        arr = json::array();
    }
    std::unordered_set<std::string> exist;
    if (arr.is_array()) {
        for (auto &e : arr) {
            if (e.is_string()) {
                exist.insert(e.get<std::string>());
            }
        }
    }
    for (auto &s : exts) {
        if (!s.empty() && !exist.count(s)) {
            arr.push_back(s);
            exist.insert(s);
        }
    }
}

static void jsonSetEnv(json &root, const std::vector<std::string> &kvs)
{
    auto &obj = root["env"];
    if (!obj.is_object()) {
        obj = json::object();
    }
    for (auto &kv : kvs) {
        auto pos = kv.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        auto k = trim(kv.substr(0, pos));
        auto v = kv.substr(pos + 1);
        if (k.empty()) {
            continue;
        }
        obj[k] = v;
    }
}

static void jsonUnsetEnv(json &root, const std::vector<std::string> &keys)
{
    if (!root.contains("env") || !root["env"].is_object()) {
        return;
    }
    for (auto &k : keys) {
        root["env"].erase(k);
    }
}

struct FsArg {
    std::string host, target, mode;
    bool persist = false;
};

static void jsonAddFsTo(json &root, const FsArg &fs, const char *field)
{
    auto &arr = root[field];
    if (!arr.is_array()) {
        arr = json::array();
    }
    for (auto &e : arr) {
        if (!e.is_object()) {
            continue;
        }
        if (e.value("host", "") == fs.host && e.value("target", "") == fs.target) {
            return;
        }
    }
    json o = json::object();
    o["host"] = fs.host;
    o["target"] = fs.target;
    o["mode"] = (fs.mode == "rw" ? "rw" : "ro");
    if (fs.persist) {
        o["persist"] = true;
    }
    arr.push_back(std::move(o));
}

static void jsonAddFs(json &root, const FsArg &fs)
{
    jsonAddFsTo(root, fs, "filesystem");
}

static void jsonAddFsAllow(json &root, const FsArg &fs)
{
    jsonAddFsTo(root, fs, "filesystem_allow_only");
}

static bool jsonRmFsByTargetFrom(json &root, const std::string &target, const char *field)
{
    if (!root.contains(field) || !root[field].is_array()) {
        return false;
    }
    auto &arr = root[field];
    auto old = arr.size();
    arr.erase(std::remove_if(arr.begin(), arr.end(), [&](const json &e) {
        return e.is_object() && e.value("target", "") == target;
    }),
              arr.end());
    return arr.size() != old;
}

static bool jsonRmFsByTarget(json &root, const std::string &target)
{
    return jsonRmFsByTargetFrom(root, target, "filesystem");
}

static bool jsonRmFsAllowByTarget(json &root, const std::string &target)
{
    return jsonRmFsByTargetFrom(root, target, "filesystem_allow_only");
}

static bool jsonRmFsByIndexFrom(json &root, size_t idx, const char *field)
{
    if (!root.contains(field) || !root[field].is_array()) {
        return false;
    }
    auto &arr = root[field];
    if (idx >= arr.size()) {
        return false;
    }
    arr.erase(arr.begin() + idx);
    return true;
}

static bool jsonRmFsByIndex(json &root, size_t idx)
{
    return jsonRmFsByIndexFrom(root, idx, "filesystem");
}

static bool jsonRmFsAllowByIndex(json &root, size_t idx)
{
    return jsonRmFsByIndexFrom(root, idx, "filesystem_allow_only");
}

static void jsonClearFsAllow(json &root)
{
    root["filesystem_allow_only"] = json::array();
}

struct CmdSetArg {
    std::string cmd;
    std::optional<std::string> entrypoint;
    std::optional<std::string> cwd;
    std::vector<std::string> argsPrefix;
    std::vector<std::string> argsSuffix;
    std::vector<std::string> envKVs;
};

static void jsonSetCommand(json &root, const CmdSetArg &a)
{
    if (a.cmd.empty()) {
        return;
    }
    auto &cmds = root["commands"];
    if (!cmds.is_object()) {
        cmds = json::object();
    }
    auto &node = cmds[a.cmd];
    if (!node.is_object()) {
        node = json::object();
    }
    if (a.entrypoint) {
        node["entrypoint"] = *a.entrypoint;
    }
    if (a.cwd) {
        node["cwd"] = *a.cwd;
    }
    if (!a.argsPrefix.empty()) {
        auto &arr = node["args_prefix"];
        arr = json::array();
        for (auto &s : a.argsPrefix) {
            arr.push_back(s);
        }
    }
    if (!a.argsSuffix.empty()) {
        auto &arr = node["args_suffix"];
        arr = json::array();
        for (auto &s : a.argsSuffix) {
            arr.push_back(s);
        }
    }
    if (!a.envKVs.empty()) {
        auto &env = node["env"];
        if (!env.is_object()) {
            env = json::object();
        }
        for (auto &kv : a.envKVs) {
            auto pos = kv.find('=');
            if (pos == std::string::npos) {
                continue;
            }
            auto k = trim(kv.substr(0, pos));
            auto v = kv.substr(pos + 1);
            if (!k.empty()) {
                env[k] = v;
            }
        }
    }
}

static void jsonUnsetCommand(json &root, const std::string &cmd)
{
    if (!root.contains("commands") || !root["commands"].is_object()) {
        return;
    }
    root["commands"].erase(cmd);
}
// ===== end: ll-cli config helpers =====

struct ConfigScopeOptions {
    bool global = false;
    std::string appId;
    std::string baseId;
};

static void addConfigScopeOptions(CLI::App *cmd, ConfigScopeOptions &opts)
{
    auto *globalFlag = cmd->add_flag("--global", opts.global, _("Operate on global configuration"));
    auto *baseOpt = cmd->add_option("--base", opts.baseId, _("Operate on base configuration"));
    auto *appOpt = cmd->add_option("--app", opts.appId, _("Operate on application configuration"))
                      ->type_name("APPID");
    baseOpt->excludes(globalFlag);
    appOpt->excludes(globalFlag);
    appOpt->excludes(baseOpt);
}

static bool resolveScopeOptions(const ConfigScopeOptions &opts,
                                Scope &scope,
                                std::string &appId,
                                std::string &baseId,
                                std::string &error)
{
    int count = (opts.global ? 1 : 0) + (!opts.appId.empty() ? 1 : 0) + (!opts.baseId.empty() ? 1 : 0);
    if (count != 1) {
        error = "specify exactly one of --global, --base or --app <appid>";
        return false;
    }
    if (opts.global) {
        scope = Scope::Global;
    } else if (!opts.baseId.empty()) {
        scope = Scope::Base;
        baseId = opts.baseId;
    } else {
        scope = Scope::App;
        appId = opts.appId;
    }
    return true;
}

static std::optional<json>
loadCliConfigFromPackage(Scope scope, const std::string &appId, const std::string &baseId)
{
    Q_UNUSED(scope);
    Q_UNUSED(appId);
    Q_UNUSED(baseId);
    return std::nullopt;
}

static std::optional<json> openConfig(Scope scope,
                                      const std::string &appId,
                                      const std::string &baseId)
{
    auto userPath = getConfigPath(scope, appId, baseId);
    if (userPath.empty()) {
        fprintf(stderr, "invalid config path\n");
        return std::nullopt;
    }
    bool userExists = false;
    auto userJson = readJsonIfExists(userPath, &userExists);
    if (!userJson) {
        fprintf(stderr, "failed to read %s\n", userPath.string().c_str());
        return std::nullopt;
    }
    if (!userExists) {
        auto searchPaths = getConfigSearchPaths(scope, appId, baseId);
        for (size_t idx = 1; idx < searchPaths.size(); ++idx) {
            bool existed = false;
            auto fallback = readJsonIfExists(searchPaths[idx], &existed);
            if (!fallback || !existed) {
                continue;
            }
            return fallback;
        }
        if (auto packaged = loadCliConfigFromPackage(scope, appId, baseId)) {
            return packaged;
        }
    }
    return userJson;
}

static bool saveConfig(Scope scope,
                       const std::string &appId,
                       const std::string &baseId,
                       const json &j)
{
    auto path = getConfigPath(scope, appId, baseId);
    if (path.empty()) {
        return false;
    }
    if (!writeJsonAtomic(path, j)) {
        fprintf(stderr, "failed to write %s\n", path.string().c_str());
        return false;
    }
    printf(_("Written %s\n"), path.string().c_str());
    return true;
}

class ConfigAwareFormatter : public CLI::Formatter {
public:
    ConfigAwareFormatter(std::string shortSection, std::string fullSection, std::string footerMessage)
      : shortSection_(std::move(shortSection))
      , fullSection_(std::move(fullSection))
      , footerMessage_(std::move(footerMessage))
    {}

    std::string make_help(const CLI::App *app, std::string name, CLI::AppFormatMode mode) const override
    {
        std::string result = Formatter::make_help(app, std::move(name), mode);

        const std::string &section = (mode == CLI::AppFormatMode::All) ? fullSection_ : shortSection_;
        if (!section.empty()) {
            if (!result.empty() && result.back() != '\n') {
                result.push_back('\n');
            }
            result += section;
            if (!section.empty() && result.back() != '\n') {
                result.push_back('\n');
            }
        }

        if (!footerMessage_.empty()) {
            if (!result.empty() && result.back() != '\n') {
                result.push_back('\n');
            }
            result += footerMessage_;
            if (result.back() != '\n') {
                result.push_back('\n');
            }
        }

        return result;
    }

private:
    std::string shortSection_;
    std::string fullSection_;
    std::string footerMessage_;
};

int runCliApplication(int argc, char **mainArgv)
{
    CLI::App commandParser{ _(
      "linyaps CLI\n"
      "A CLI program to run application and manage application and runtime\n") };
    auto shortConfigHelp = configShortHelp();
    auto fullConfigHelp = shortConfigHelp + configUsageLines();
    commandParser.formatter(std::make_shared<ConfigAwareFormatter>(shortConfigHelp,
                                                                   fullConfigHelp,
                                                                   configFooterMessage()));
    commandParser.option_defaults()->group(_("Options"));
    if (auto formatter = commandParser.get_formatter()) {
        formatter->label("OPTIONS", _("OPTIONS"));
        formatter->label("SUBCOMMAND", _("SUBCOMMAND"));
        formatter->label("SUBCOMMANDS", _("SUBCOMMANDS"));
        formatter->label("POSITIONALS", _("POSITIONALS"));
        formatter->label("Usage", _("Usage"));
        formatter->label("REQUIRED", _("REQUIRED"));
    }
    auto argv = commandParser.ensure_utf8(mainArgv);
    if (argc == 1) {
        std::cout << commandParser.help() << std::endl;
        return 0;
    }

    if (auto *helpOption = commandParser.get_help_ptr()) {
        helpOption->description(_("Print this help message and exit"));
        helpOption->group(_("Options"));
    }
    if (auto *helpAllOption = commandParser.set_help_all_flag("--help-all", _("Expand all help"))) {
        helpAllOption->group(_("Options"));
    }
    commandParser.usage(_("Usage: ll-cli [OPTIONS] [SUBCOMMAND]"));
    commandParser.footer("");

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

    // groups for subcommands
    auto *CliBuildInGroup = _("Managing installed applications and runtimes");
    auto *CliAppManagingGroup = _("Managing running applications");
    auto *CliSearchGroup = _("Finding applications and runtimes");
    auto *CliRepoGroup = _("Managing remote repositories");

    bool configHandled = false;
    int configResult = 0;

    auto *configCmd = commandParser.add_subcommand("config", _("Manage ll-cli configuration"));
    configCmd->require_subcommand();
    configCmd->group(_("Configuration"));

    auto resolveScopeOrThrow = [&](const ConfigScopeOptions &opts) {
        Scope scope = Scope::Global;
        std::string appId, baseId, error;
        if (!resolveScopeOptions(opts, scope, appId, baseId, error)) {
            throw CLI::ValidationError("scope", error);
        }
        return std::make_tuple(scope, appId, baseId);
    };

    auto handleConfigResult = [&](bool ok) {
        configHandled = true;
        configResult = ok ? 0 : 1;
    };

    // config set-extensions / add-extensions
    auto addExtensionsCommand = [&](const char *name, bool overwrite) {
        auto *sub = configCmd->add_subcommand(name, _("Manage default extensions"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto exts = std::make_shared<std::string>();
        sub->add_option("extensions",
                        *exts,
                        _("Comma separated extensions, e.g. ext1,ext2"))->required();
        sub->callback([&, scopeOpts, exts, overwrite]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonSetExtensions(*j, splitCsv(*exts), overwrite);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    };
    addExtensionsCommand("set-extensions", true);
    addExtensionsCommand("add-extensions", false);

    // config set-env
    {
        auto *sub = configCmd->add_subcommand("set-env",
                                              _("Set environment variables for target scope"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto kvs = std::make_shared<std::vector<std::string>>();
        sub->add_option("env", *kvs, _("KEY=VALUE entries"))->required()->expected(1, -1);
        sub->callback([&, scopeOpts, kvs]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonSetEnv(*j, *kvs);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    }

    // config unset-env
    {
        auto *sub = configCmd->add_subcommand("unset-env",
                                              _("Unset environment variables for target scope"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto keys = std::make_shared<std::vector<std::string>>();
        sub->add_option("keys", *keys, _("Environment variable keys"))->required()->expected(1, -1);
        sub->callback([&, scopeOpts, keys]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonUnsetEnv(*j, *keys);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    }

    // config add-fs / add-fs-allow
    auto addFsCommand = [&](const char *name, auto inserter) {
        auto *sub = configCmd->add_subcommand(name, _("Add filesystem entry"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto fs = std::make_shared<FsArg>();
        fs->mode = "ro";
        sub->add_option("--host", fs->host, _("Host path to mount"))->required();
        sub->add_option("--target", fs->target, _("Target path inside container"))->required();
        sub->add_option("--mode", fs->mode, _("Mount mode (ro|rw)"))
          ->check(CLI::IsMember({ "ro", "rw" }))
          ->default_str("ro");
        sub->add_flag("--persist", fs->persist, _("Persist mount under sandbox storage"));
        sub->callback([&, scopeOpts, fs, inserter]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            inserter(*j, *fs);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    };
    addFsCommand("add-fs", jsonAddFs);
    addFsCommand("add-fs-allow", jsonAddFsAllow);

    // config rm-fs / rm-fs-allow
    auto addRemoveFsCommand = [&](const char *name, auto removeTarget, auto removeIndex) {
        auto *sub = configCmd->add_subcommand(name, _("Remove filesystem entry"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto target = std::make_shared<std::optional<std::string>>();
        auto indexStr = std::make_shared<std::optional<std::string>>();
        sub->add_option("--target", *target, _("Target path inside container"));
        sub->add_option("--index", *indexStr, _("Index of entry in list"));
        sub->callback([&, scopeOpts, target, indexStr, removeTarget, removeIndex]() {
            if (!*target && !*indexStr) {
                throw CLI::ValidationError("target/index",
                                           "either --target or --index must be provided");
            }
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            bool ok = false;
            if (*target) {
                ok = removeTarget(*j, **target);
            }
            if (!ok && *indexStr) {
                size_t parsedIndex = 0;
                try {
                    parsedIndex = static_cast<size_t>(std::stoul(indexStr->value()));
                } catch (const std::exception &) {
                    fprintf(stderr, "Invalid index value: %s\n", indexStr->value().c_str());
                    handleConfigResult(false);
                    return;
                }
                ok = removeIndex(*j, parsedIndex);
            }
            if (!ok) {
                fprintf(stderr, "no filesystem entry removed\n");
                handleConfigResult(false);
                return;
            }
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    };
    addRemoveFsCommand("rm-fs", jsonRmFsByTarget, jsonRmFsByIndex);
    addRemoveFsCommand("rm-fs-allow", jsonRmFsAllowByTarget, jsonRmFsAllowByIndex);

    // config clear-fs-allow
    {
        auto *sub = configCmd->add_subcommand("clear-fs-allow",
                                              _("Clear filesystem allowlist"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        sub->callback([&, scopeOpts]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonClearFsAllow(*j);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    }

    // config set-command
    {
        auto *sub = configCmd->add_subcommand("set-command",
                                              _("Set per-command overrides (env, args etc.)"));
        sub->allow_extras();
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto arg = std::make_shared<CmdSetArg>();
        sub->add_option("command", arg->cmd, _("Command name"))->required();

        auto entrypoint = std::make_shared<std::string>();
        auto cwd = std::make_shared<std::string>();
        auto argsPrefixRaw = std::make_shared<std::string>();
        auto argsSuffixRaw = std::make_shared<std::string>();
        auto envPairs = std::make_shared<std::vector<std::string>>();

        sub->add_option("--entrypoint", *entrypoint, _("Override entrypoint"));
        sub->add_option("--cwd", *cwd, _("Working directory"));
        sub->add_option("--args-prefix", *argsPrefixRaw, _("Arguments prepended before command"));
        sub->add_option("--args-suffix", *argsSuffixRaw, _("Arguments appended after command"));
        sub->add_option("--env", *envPairs, _("Environment entries (KEY=VAL)"))->expected(0, -1);

        sub->callback([&, sub, scopeOpts, arg, entrypoint, cwd, argsPrefixRaw, argsSuffixRaw, envPairs]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            arg->entrypoint.reset();
            arg->cwd.reset();
            arg->argsPrefix.clear();
            arg->argsSuffix.clear();
            arg->envKVs.clear();

            if (!entrypoint->empty()) {
                arg->entrypoint = *entrypoint;
            }
            if (!cwd->empty()) {
                arg->cwd = *cwd;
            }
            if (!argsPrefixRaw->empty()) {
                std::stringstream ss(*argsPrefixRaw);
                std::string tok;
                while (ss >> tok) {
                    arg->argsPrefix.push_back(tok);
                }
            }
            if (!argsSuffixRaw->empty()) {
                std::stringstream ss(*argsSuffixRaw);
                std::string tok;
                while (ss >> tok) {
                    arg->argsSuffix.push_back(tok);
                }
            }
            arg->envKVs.insert(arg->envKVs.end(), envPairs->begin(), envPairs->end());
            auto extras = sub->remaining_for_passthrough();
            arg->envKVs.insert(arg->envKVs.end(), extras.begin(), extras.end());

            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonSetCommand(*j, *arg);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    }

    // config unset-command
    {
        auto *sub = configCmd->add_subcommand("unset-command",
                                              _("Remove per-command overrides"));
        auto scopeOpts = std::make_shared<ConfigScopeOptions>();
        addConfigScopeOptions(sub, *scopeOpts);
        auto cmd = std::make_shared<std::string>();
        sub->add_option("command", *cmd, _("Command name"))->required();
        sub->callback([&, scopeOpts, cmd]() {
            auto [scope, appId, baseId] = resolveScopeOrThrow(*scopeOpts);
            auto j = openConfig(scope, appId, baseId);
            if (!j) {
                handleConfigResult(false);
                return;
            }
            jsonUnsetCommand(*j, *cmd);
            handleConfigResult(saveConfig(scope, appId, baseId, *j));
        });
    }

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

    auto res = transformOldExec(argc, argv);
    CLI11_PARSE(commandParser, std::move(res));

    if (configCmd->parsed()) {
        return configResult;
    }

    // print version if --version flag is set
    if (*versionFlag) {
        if (*jsonFlag) {
            std::cout << nlohmann::json{ { "version", LINGLONG_VERSION } } << std::endl;
        } else {
            std::cout << _("linyaps CLI version ") << LINGLONG_VERSION << std::endl;
        }
        return 0;
    }
    // set log level if --verbose flag is set
    if (globalOptions.verbose) {
        linglong::utils::log::setLogLevel(linglong::utils::log::LogLevel::Debug);
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
    if (*noDBusFlag) {
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
    auto path = QStandardPaths::findExecutable(ociRuntimeCLI, { BINDIR });
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
        result = cli->inspect(*ret, inspectOptions);
    } else if (name == "repo") {
        result = cli->repo(*ret, repoOptions);
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
    initLinyapsLogSystem(linglong::utils::log::LogBackend::Journal);

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
