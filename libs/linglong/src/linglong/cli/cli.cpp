/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "configure.h"
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/LinglongAPIV1.hpp"
#include "linglong/api/types/v1/PackageInfoDisplay.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1PackageTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1PruneResult.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/cli/printer.h"
#include "linglong/common/dir.h"
#include "linglong/common/strings.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/package/layer_file.h"
#include "linglong/package/reference.h"
#include "linglong/repo/config.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/xdg/directory.h"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <linux/un.h>
#include <nlohmann/json.hpp>

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

using namespace linglong::utils::error;

namespace {

std::vector<std::string> getAutoModuleList() noexcept
{
    auto getModuleFromLanguageEnv = [](const std::string &lang) -> std::vector<std::string> {
        if (lang.length() < 2) {
            return {};
        }

        if (!std::all_of(lang.begin(), lang.begin() + 2, [](char c) {
                return 'a' <= c && c <= 'z';
            })) {
            return {};
        }

        std::vector<std::string> modules;
        modules.push_back("lang_" + lang.substr(0, 2));

        if (lang.length() == 2) {
            return modules;
        }

        if (lang[2] == '.') {
            return modules;
        }

        if (lang[2] == '@') {
            return modules;
        }

        if (lang[2] != '_') {
            return {};
        }

        if (lang.length() < 5) {
            return {};
        }

        modules.push_back("lang_" + lang.substr(0, 5));

        if (lang.length() == 5) {
            return modules;
        }

        if (lang[5] == '.') {
            return modules;
        }

        if (lang[5] == '@') {
            return modules;
        }

        return {};
    };

    auto envs = {
        "LANG",           "LC_ADDRESS",  "LC_ALL",       "LC_IDENTIFICATION",
        "LC_MEASUREMENT", "LC_MESSAGES", "LC_MONETARY",  "LC_NAME",
        "LC_NUMERIC",     "LC_PAPER",    "LC_TELEPHONE", "LC_TIME",
    };

    std::vector<std::string> result = { "binary" };

    for (const auto &env : envs) {
        auto lang = getenv(env);
        if (lang == nullptr) {
            continue;
        }
        auto modules = getModuleFromLanguageEnv(lang);
        result.insert(result.end(), modules.begin(), modules.end());
    }

    std::sort(result.begin(), result.end());
    return { result.begin(), std::unique(result.begin(), result.end()) };
}

bool delegateToContainerInit(const std::string &containerID,
                             std::vector<std::string> commands) noexcept
{
    auto containerSocket = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (containerSocket == -1) {
        return false;
    }

    auto cleanup = linglong::utils::finally::finally([containerSocket] {
        ::close(containerSocket);
    });

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;

    auto bundleDir = linglong::common::dir::getBundleDir(containerID);
    const std::string socketPath = bundleDir / "init/socket";

    std::copy(socketPath.begin(), socketPath.end(), &addr.sun_path[0]);
    addr.sun_path[socketPath.size() + 1] = 0;

    auto ret = ::connect(containerSocket,
                         reinterpret_cast<struct sockaddr *>(&addr),
                         offsetof(sockaddr_un, sun_path) + socketPath.size());
    if (ret == -1) {
        return false;
    }

    std::string bashContent;
    for (const auto &command : commands) {
        bashContent.append(linglong::common::strings::quoteBashArg(command));
        bashContent.push_back(' ');
    }

    commands.clear();
    commands.push_back(bashContent);
    commands.insert(commands.begin(), "-c");
    commands.insert(commands.begin(), "--login");
    commands.insert(commands.begin(), "bash");

    std::string command_data;
    for (const auto &command : commands) {
        command_data.append(command);
        command_data.push_back('\0');
    }
    command_data.push_back('\0');

    std::uint64_t rest_len = command_data.size();
    ret = ::send(containerSocket, &rest_len, sizeof(rest_len), 0);
    if (ret == -1) {
        return false;
    }

    while (rest_len > 0) {
        auto send_len = ::send(containerSocket, command_data.c_str(), rest_len, 0);
        if (send_len == -1) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        rest_len -= send_len;
    }

    if (rest_len != 0) {
        return false;
    }

    int result{ -1 };
    ret = ::recv(containerSocket, &result, sizeof(result), 0);
    qDebug() << "delegate result:" << result;
    return result == 0;
}

} // namespace

namespace linglong::cli {

void Cli::onTaskPropertiesChanged(
  const QString &interface,                                   // NOLINT
  const QVariantMap &changed_properties,                      // NOLINT
  [[maybe_unused]] const QStringList &invalidated_properties) // NOLINT
{
    if (interface != task->interface()) {
        return;
    }

    for (auto entry = changed_properties.cbegin(); entry != changed_properties.cend(); ++entry) {
        const auto &key = entry.key();
        const auto &value = entry.value();

        if (key == "State") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                qCritical() << "dbus ipc error, State couldn't convert to int";
                continue;
            }

            taskState.state = static_cast<api::types::v1::State>(val);
            continue;
        }

        if (key == "Percentage") {
            bool ok{ false };
            auto val = value.toDouble(&ok);
            if (!ok) {
                qCritical() << "dbus ipc error, Percentage couldn't convert to int";
                continue;
            }

            taskState.percentage = val > 100 ? 100 : val;
            continue;
        }

        if (key == "Message") {
            if (!value.canConvert<QString>()) {
                qCritical() << "dbus ipc error, Message couldn't convert to QString";
                continue;
            }

            taskState.message = value.toString().toStdString();
            continue;
        }

        if (key == "Code") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                qCritical() << "dbus ipc error, Code couldn't convert to int";
                continue;
            }

            taskState.errorCode = static_cast<utils::error::ErrorCode>(val);
        }
    }

    handleTaskState();
}

void Cli::interaction(const QDBusObjectPath &object_path,
                      int messageID,
                      const QVariantMap &additionalMessage)
{
    LINGLONG_TRACE("interactive with user")
    if (object_path.path() != taskObjectPath) {
        return;
    }

    auto messageType = static_cast<api::types::v1::InteractionMessageType>(messageID);
    auto msg = utils::serialize::fromQVariantMap<
      api::types::v1::PackageManager1RequestInteractionAdditionalMessage>(additionalMessage);

    std::vector<std::string> actions{ "yes", "Yes", "no", "No" };

    api::types::v1::InteractionRequest req;
    req.actions = actions;
    req.summary = "Package Manager needs to confirm request.";

    switch (messageType) {
    case api::types::v1::InteractionMessageType::Upgrade: {
        auto tips =
          QString("The lower version %1 is currently installed. Do you "
                  "want to continue installing the latest version %2?")
            .arg(QString::fromStdString(msg->localRef), QString::fromStdString(msg->remoteRef));
        req.body = tips.toStdString();
    } break;
    case api::types::v1::InteractionMessageType::Downgrade:
    case api::types::v1::InteractionMessageType::Install:
    case api::types::v1::InteractionMessageType::Uninstall:
        [[fallthrough]];
    case api::types::v1::InteractionMessageType::Unknown:
        // reserve for future use
        req.body = "unknown interaction type";
        break;
    }

    std::string action;
    auto notifyReply = notifier->request(req);
    if (!notifyReply) {
        qCritical() << "internal error: notify failed";
        action = "no";
    } else {
        action = notifyReply->action.value();
    }

    // FIXME: if the notifier is a DummyNotifier, treat the action as yes.(for deepin-app-store)
    // But this behavior is no correct. We should treat it as no and tell people to add additional
    // option '-y'.
    if (action == "Y" || action == "y" || action == "Yes" || action == "yes" || action == "dummy") {
        action = "yes";
    } else {
        action = "no";
    }

    qDebug() << "action: " << QString::fromStdString(action);

    auto reply = api::types::v1::InteractionReply{ .action = action };

    QDBusPendingReply<void> dbusReply =
      this->pkgMan.ReplyInteraction(object_path, utils::serialize::toQVariantMap(reply));
    dbusReply.waitForFinished();
    if (dbusReply.isError()) {
        this->printer.printErr(
          LINGLONG_ERRV(dbusReply.error().message(), dbusReply.error().type()));
    }
}

void Cli::onTaskAdded(const QDBusObjectPath &object_path)
{
    LogD("task added: {}", object_path.path());
}

void Cli::onTaskRemoved(const QDBusObjectPath &object_path)
{
    LogD("task removed: {}", object_path.path());
    if (object_path.path() != taskObjectPath) {
        return;
    }

    delete task;
    task = nullptr;
    Q_EMIT taskDone();
}

void Cli::handleTaskState() noexcept
{
    if (taskState.state == api::types::v1::State::Unknown) {
        LogW("task state is unknown");
        return;
    }

    if (taskState.state == api::types::v1::State::Failed
        || taskState.state == api::types::v1::State::Canceled) {
        this->printer.clearLine();
        this->printOnTaskFailed();
        return;
    }

    if (taskState.state == api::types::v1::State::Succeed) {
        this->printer.clearLine();
        this->printOnTaskSuccess();
        return;
    }

    if (!this->globalOptions.noProgress) {
        this->printer.printProgress(taskState.percentage, taskState.message);
    }
}

void Cli::printOnTaskFailed()
{
    LINGLONG_TRACE("cli handle task failed");

    auto error = LINGLONG_ERRV(taskState.message, taskState.errorCode);

    switch (taskState.taskType) {
    case TaskType::Install:
        handleInstallError(
          error,
          std::get<api::types::v1::PackageManager1InstallParameters>(taskState.params));
        break;
    case TaskType::Uninstall:
        handleUninstallError(error);
        break;
    case TaskType::Upgrade:
        handleUpgradeError(error);
        break;
    default:
        handleCommonError(error);
        break;
    }
}

void Cli::printOnTaskSuccess()
{
    this->printer.printMessage(taskState.message);
}

Cli::Cli(Printer &printer,
         ocppi::cli::CLI &ociCLI,
         runtime::ContainerBuilder &containerBuilder,
         api::dbus::v1::PackageManager &pkgMan,
         repo::OSTreeRepo &repo,
         std::unique_ptr<InteractiveNotifier> &&notifier,
         QObject *parent)
    : QObject(parent)
    , printer(printer)
    , ociCLI(ociCLI)
    , containerBuilder(containerBuilder)
    , repository(repo)
    , notifier(std::move(notifier))
    , pkgMan(pkgMan)
{
    auto conn = pkgMan.connection();
    if (!conn.connect(pkgMan.service(),
                      pkgMan.path(),
                      pkgMan.interface(),
                      "TaskAdd",
                      this,
                      SLOT(onTaskAdded(QDBusObjectPath)))) {
        LogE("couldn't connect to package manager signal 'TaskAdded'");
    }

    if (!conn.connect(pkgMan.service(),
                      pkgMan.path(),
                      pkgMan.interface(),
                      "TaskRemoved",
                      this,
                      SLOT(onTaskRemoved(QDBusObjectPath)))) {
        LogE("couldn't connect to package manager signal 'TaskRemoved'");
    }
}

int Cli::run(const RunOptions &options)
{
    LINGLONG_TRACE("command run");

    auto uid = getuid();
    auto gid = getgid();
    auto pid = getpid();

    detectDrivers();

    auto userContainerDir = std::filesystem::path{ "/run/linglong" } / std::to_string(uid);
    if (auto ret = utils::ensureDirectory(userContainerDir); !ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    auto pidFile = userContainerDir / std::to_string(pid);
    // placeholder file
    auto fd = ::open(pidFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd == -1) {
        qCritical()
          << QString{ "create file %1 error: %2" }.arg(pidFile.c_str()).arg(::strerror(errno));
        QCoreApplication::exit(-1);
        return -1;
    }
    ::close(fd);

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [pidFile] {
        std::error_code ec;
        if (!std::filesystem::remove(pidFile, ec) && ec) {
            qCritical().nospace() << "failed to remove file " << pidFile.c_str() << ": "
                                  << ec.message().c_str();
        }
    });

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto curAppRef = this->repository.clearReference(*fuzzyRef,
                                                     {
                                                       .forceRemote = false,
                                                       .fallbackToRemote = false,
                                                     });
    if (!curAppRef) {
        this->printer.printErr(curAppRef.error());
        return -1;
    }

    runtime::RunContext runContext(this->repository);
    linglong::runtime::ResolveOptions opts;
    opts.baseRef = options.base;
    opts.runtimeRef = options.runtime;
    // 处理多个扩展
    if (!options.extensions.empty()) {
        opts.extensionRefs = options.extensions;
    }

    // 调整日志输出，打印扩展列表（用逗号拼接）
    std::string extStr =
      opts.extensionRefs ? linglong::common::strings::join(*opts.extensionRefs, ',') : "null";
    LogD("start resolve run context with base {}, runtime {}, extensions {}",
         opts.baseRef.value_or("null"),
         opts.runtimeRef.value_or("null"),
         extStr);

    auto res = runContext.resolve(*curAppRef, opts);
    if (!res) {
        handleCommonError(res.error());
        return -1;
    }

    LogD("resolved run context with base {}", runContext.getBaseLayerPath()->string());
    if (runContext.hasRuntime()) {
        LogD("resolved run context with runtime {}", runContext.getRuntimeLayerPath()->string());
    }

    const auto &appLayerItem = runContext.getCachedAppItem();
    if (!appLayerItem) {
        return -1;
    }
    const auto &info = appLayerItem->info;

    auto ret = RequestDirectories(info);
    if (!ret) {
        qWarning() << ret.error().message();
    }

    auto commands = options.commands;
    if (options.commands.empty()) {
        commands = info.command.value_or(std::vector<std::string>{ "bash" });
    }
    commands = filePathMapping(commands, options);

    // this lambda will dump reference of containerID, app, base and runtime to
    // /run/linglong/getuid()/getpid() to store these needed infomation
    auto dumpContainerInfo = [&pidFile, &runContext, this]() -> bool {
        LINGLONG_TRACE("dump info")
        std::error_code ec;
        if (!std::filesystem::exists(pidFile, ec)) {
            if (ec) {
                qCritical() << "couldn't get status of" << pidFile.c_str() << ":"
                            << ec.message().c_str();
                return false;
            }

            auto msg = "state file " + pidFile.string() + "doesn't exist, abort.";
            qFatal("%s", msg.c_str());
        }

        std::ofstream stream{ pidFile };
        if (!stream.is_open()) {
            auto msg = QString{ "failed to open %1" }.arg(pidFile.c_str());
            this->printer.printErr(LINGLONG_ERRV(msg));
            return false;
        }
        stream << nlohmann::json(runContext.stateInfo());
        stream.close();

        return true;
    };

    auto containers = getCurrentContainers().value_or(std::vector<api::types::v1::CliContainer>{});
    for (const auto &container : containers) {
        qDebug() << "found running container: " << container.package.c_str();
        if (container.package != curAppRef->toString()) {
            continue;
        }

        if (!dumpContainerInfo()) {
            return -1;
        }

        if (delegateToContainerInit(container.id, commands)) {
            return 0;
        }

        // fallback to run
        break;
    }

    auto *homeEnv = ::getenv("HOME");
    if (homeEnv == nullptr) {
        qCritical() << "Couldn't get HOME env.";
        return -1;
    }

    runContext.enableSecurityContext(runtime::getDefaultSecurityContexts());

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    cfgBuilder.setAppId(curAppRef->id)
      .setAnnotation(generator::ANNOTATION::LAST_PID, std::to_string(pid))
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindDefault()
      .bindDevNode()
      .bindCgroup()
      .bindXDGRuntime()
      .bindUserGroup()
      .bindRemovableStorageMounts()
      .bindHostRoot()
      .bindHostStatics()
      .bindHome(homeEnv)
      .enablePrivateDir()
      .mapPrivate(std::string{ homeEnv } + "/.ssh", true)
      .mapPrivate(std::string{ homeEnv } + "/.gnupg", true)
      .bindIPC()
      .forwardDefaultEnv()
      .enableSelfAdjustingMount();

    std::vector<std::string> capabilities;
    // privileged mode shares host's user_namespace and add capabilities
    if (options.privileged) {
        if (uid != 0) {
            this->printer.printMessage(_("privileged mode requires running as root"));
            return -1;
        }

        cfgBuilder.disableUserNamespace();
        capabilities = { "CAP_CHOWN",    "CAP_DAC_OVERRIDE",     "CAP_FOWNER",     "CAP_FSETID",
                         "CAP_KILL",     "CAP_NET_BIND_SERVICE", "CAP_SETFCAP",    "CAP_SETGID",
                         "CAP_SETPCAP",  "CAP_SETUID",           "CAP_SYS_CHROOT", "CAP_NET_RAW",
                         "CAP_NET_ADMIN" };
    }

    if (!options.capsAdd.empty()) {
        capabilities.insert(capabilities.end(), options.capsAdd.begin(), options.capsAdd.end());
    }

    cfgBuilder.setCapabilities(capabilities);

    res = runContext.fillContextCfg(cfgBuilder);
    if (!res) {
        this->printer.printErr(res.error());
        return -1;
    }

    std::error_code ec;
    auto socketDir = cfgBuilder.getBundlePath() / "init";
    std::filesystem::create_directories(socketDir, ec);
    if (ec) {
        this->printer.printErr(LINGLONG_ERRV(ec.message().c_str()));
        return -1;
    }

    cfgBuilder.addExtraMount(
      ocppi::runtime::config::types::Mount{ .destination = "/run/linglong/init",
                                            .options = std::vector<std::string>{ "bind" },
                                            .source = socketDir.string(),
                                            .type = "bind" });

    for (const auto &env : options.envs) {
        auto split = env.cbegin() + env.find('='); // already checked by CLI
        cfgBuilder.appendEnv(std::string(env.cbegin(), split),
                             std::string(split + 1, env.cend()),
                             true);
    }

    auto appCache = this->ensureCache(runContext, cfgBuilder);
    if (!appCache) {
        this->printer.printErr(LINGLONG_ERRV(appCache));
        return -1;
    }
    cfgBuilder.setAppCache(*appCache).enableLDCache();

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        qCritical() << "build cfg error: " << QString::fromStdString(err.reason);
        return -1;
    }

    auto container = this->containerBuilder.create(cfgBuilder);
    if (!container) {
        this->printer.printErr(container.error());
        return -1;
    }

    if (!dumpContainerInfo()) {
        return -1;
    }

    ocppi::runtime::RunOption opt{};
    auto result =
      (*container)->run(ocppi::runtime::config::types::Process{ .args = std::move(commands) }, opt);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    return 0;
}

int Cli::enter(const EnterOptions &options)
{
    LINGLONG_TRACE("ll-cli exec");
    auto containerIDList = this->getRunningAppContainers(options.instance);

    if (containerIDList.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    if (containerIDList.size() > 1) {
        this->printer.printErr(
          LINGLONG_ERRV("multiple running containers found, please specify which one to enter"));
        return -1;
    }

    auto containerID = containerIDList.front();
    qInfo() << "select container id" << QString::fromStdString(containerID);
    auto commands = options.commands;
    if (commands.empty()) {
        commands = utils::BashCommandHelper::generateDefaultBashCommand();
    }

    auto opt = ocppi::runtime::ExecOption{
        .uid = ::getuid(),
        .gid = ::getgid(),
    };

    auto result =
      this->ociCLI.exec(containerID, commands[0], { commands.begin() + 1, commands.end() }, opt);
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

utils::error::Result<std::vector<api::types::v1::CliContainer>>
Cli::getCurrentContainers() const noexcept
{
    LINGLONG_TRACE("get current running containers")

    auto containersRet = this->ociCLI.list();
    if (!containersRet) {
        return LINGLONG_ERR(containersRet);
    }
    auto containers = std::move(containersRet).value();

    std::vector<api::types::v1::CliContainer> myContainers;
    auto infoDir = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());

    std::error_code ec;
    auto it = std::filesystem::directory_iterator{ infoDir, ec };
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            return myContainers;
        }

        return LINGLONG_ERR(
          QString{ "failed to list %1: %2" }.arg(infoDir.c_str(), ec.message().c_str()));
    }

    for (const auto &pidFile : it) {
        const auto &file = pidFile.path();
        const auto &process = "/proc" / file.filename();

        std::error_code ec;
        if (!std::filesystem::exists(process, ec)) {
            // this process may exit abnormally, skip it.
            qDebug() << process.c_str() << "doesn't exist";
            continue;
        }

        if (pidFile.file_size(ec) == 0) {
            continue;
        }

        auto info = linglong::utils::serialize::LoadJSONFile<
          linglong::api::types::v1::ContainerProcessStateInfo>(file);
        if (!info) {
            qDebug() << "load info from" << file.c_str() << "error:" << info.error().message();
            continue;
        }

        auto container = std::find_if(containers.begin(),
                                      containers.end(),
                                      [&info](const ocppi::types::ContainerListItem &item) {
                                          return item.id == info->containerID;
                                      });
        if (container == containers.cend()) {
            qDebug() << "couldn't find container that process " << file.filename().c_str()
                     << "belongs to";
            continue;
        }

        myContainers.emplace_back(api::types::v1::CliContainer{
          .id = std::move(info->containerID),
          .package = std::move(info->app),
          .pid = container->pid,
        });
    }

    return myContainers;
}

int Cli::ps()
{
    auto myContainers = getCurrentContainers();
    if (!myContainers) {
        this->printer.printErr(myContainers.error());
        return -1;
    }

    // TODO: add option --no-truncated
    std::for_each(myContainers->begin(),
                  myContainers->end(),
                  [](api::types::v1::CliContainer &container) {
                      container.id = container.id.substr(0, 12);
                  });

    this->printer.printContainers(*myContainers);

    return 0;
}

std::vector<std::string> Cli::getRunningAppContainers(const std::string &appid)
{
    LINGLONG_TRACE("get app running containers");

    std::vector<std::string> containerIDList{};
    auto containers = getCurrentContainers();
    if (!containers) {
        this->printer.printErr(containers.error());
        return containerIDList;
    }

    for (const auto &container : *containers) {
        auto fuzzyRef = package::FuzzyReference::parse(container.package);
        if (!fuzzyRef) {
            qWarning() << LINGLONG_ERRV(fuzzyRef).message();
            continue;
        }

        if (fuzzyRef->id == appid || fuzzyRef->toString() == appid) {
            containerIDList.emplace_back(container.id);
        }
    }

    return containerIDList;
}

int Cli::kill(const KillOptions &options)
{
    LINGLONG_TRACE("command kill");

    auto containerIDList = this->getRunningAppContainers(options.appid);

    auto ret = 0;
    for (const auto &containerID : containerIDList) {
        qInfo() << "select container id" << QString::fromStdString(containerID);
        auto result = this->ociCLI.kill(ocppi::runtime::ContainerID(containerID),
                                        ocppi::runtime::Signal(options.signal));
        if (!result) {
            auto err = LINGLONG_ERRV(result);
            this->printer.printErr(err);
            ret = -1;
        }
    }

    return ret;
}

void Cli::cancelCurrentTask()
{
    if (this->task != nullptr) {
        LogD("cancel running task");
        this->task->Cancel();
    }
}

int Cli::installFromFile(const QFileInfo &fileInfo,
                         const api::types::v1::CommonOptions &commonOptions,
                         const std::string &appid)
{
    auto filePath = fileInfo.absoluteFilePath();
    LINGLONG_TRACE(fmt::format("install from file {}", filePath.toStdString()));

    auto authReply = this->authorization();
    if (!authReply.isValid()) {
        if (authReply.error().type() == QDBusError::AccessDenied) {
            auto args = QCoreApplication::instance()->arguments();
            // pkexec在0.120版本之前没有keep-cwd选项，会将目录切换到/root
            // 所以将layer或uab文件的相对路径转为绝对路径，再传给pkexec
            auto path = fileInfo.absoluteFilePath();
            for (auto i = 0; i < args.length(); i++) {
                if (args[i] == QString::fromStdString(appid)) {
                    args[i] = path.toLocal8Bit().constData();
                }
            }

            auto ret = this->runningAsRoot(args);
            if (!ret) {
                this->printer.printErr(ret.error());
            }
            return -1;
        }

        this->printer.printErr(LINGLONG_ERRV(authReply.error().message() + authReply.error().name(),
                                             static_cast<int>(authReply.error().type())));
        return -1;
    }

    auto res = this->initInteraction();
    if (!res) {
        this->printer.printErr(res.error());
        return -1;
    }

    LogI("install from file {}", filePath);
    QFile file{ filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        auto err = LINGLONG_ERR(file);
        this->printer.printErr(err.value());
        return -1;
    }

    QDBusUnixFileDescriptor dbusFileDescriptor(file.handle());

    auto pendingReply =
      this->pkgMan.InstallFromFile(dbusFileDescriptor,
                                   fileInfo.suffix(),
                                   utils::serialize::toQVariantMap(commonOptions));
    res = waitTaskCreated(pendingReply, TaskType::InstallFromFile);
    if (!res) {
        this->handleCommonError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::install(const InstallOptions &options)
{
    LINGLONG_TRACE("command install");

    auto params =
      api::types::v1::PackageManager1InstallParameters{ .options = { .force = false,
                                                                     .skipInteraction = false } };
    params.options.force = options.forceOpt;
    params.options.skipInteraction = options.confirmOpt;
    params.repo = options.repo;

    QFileInfo info(QString::fromStdString(options.appid));

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(QFileInfo{ info.absoluteFilePath() }, params.options, options.appid);
    }

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    ret = this->initInteraction();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }

    if (!options.module.empty()) {
        params.package.modules = { options.module };
    } else {
        params.package.modules = getAutoModuleList();
    }

    LogD("install module: {}", common::strings::join(*params.package.modules));

    auto pendingReply = this->pkgMan.Install(utils::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Install);
    if (!res) {
        handleInstallError(res.error(), params);
        return -1;
    }
    this->taskState.params = std::move(params);

    waitTaskDone();

    updateAM();
    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::upgrade(const UpgradeOptions &options)
{
    LINGLONG_TRACE("command upgrade");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    std::vector<package::Reference> toUpgrade;
    if (!options.appid.empty()) {
        auto fuzzyRef = package::FuzzyReference::parse(options.appid);
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto localRef = this->repository.clearReference(*fuzzyRef,
                                                        {
                                                          .forceRemote = false,
                                                          .fallbackToRemote = false,
                                                        });
        if (!localRef) {
            this->printer.printMessage(
              fmt::format(_("Application {} is not installed."), options.appid));
            return -1;
        }

        auto layerItemRet = this->repository.getLayerItem(*localRef);
        if (!layerItemRet) {
            this->printer.printErr(layerItemRet.error());
            return -1;
        }
        if (layerItemRet->info.kind != "app") {
            this->printer.printMessage(fmt::format(_("{} is not an application."), options.appid));
            return -1;
        }
        toUpgrade.emplace_back(std::move(localRef).value());
    }

    api::types::v1::PackageManager1UpdateParameters params;
    params.depsOnly = options.depsOnly;
    for (const auto &ref : toUpgrade) {
        api::types::v1::PackageManager1Package package;
        package.id = ref.id;
        package.channel = ref.channel;
        params.packages.emplace_back(std::move(package));
    }

    auto pendingReply = this->pkgMan.Update(utils::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Upgrade);
    if (!res) {
        handleUpgradeError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::search(const SearchOptions &options)
{
    LINGLONG_TRACE("command search");

    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = options.appid,
        .repos = {},
    };

    auto repoConfig = this->repository.getOrderedConfig();
    if (repoConfig.repos.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no repo found"));
        return -1;
    }

    if (options.repo) {
        auto it = std::find_if(repoConfig.repos.begin(),
                               repoConfig.repos.end(),
                               [&options](const api::types::v1::Repo &repo) {
                                   return repo.alias.value_or(repo.name) == options.repo.value();
                               });
        if (it == repoConfig.repos.end()) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("repo {} not found", options.repo.value())));
            return -1;
        }
        params.repos.emplace_back(options.repo.value());
    } else {
        // search all repos
        for (const auto &repo : repoConfig.repos) {
            params.repos.emplace_back(repo.alias.value_or(repo.name));
        }
    }

    std::optional<QString> pendingJobID;

    QEventLoop loop;
    connect(
      &this->pkgMan,
      &api::dbus::v1::PackageManager::SearchFinished,
      [&pendingJobID, this, &loop, &options](const QString &jobID, const QVariantMap &data) {
          LINGLONG_TRACE("process search result");
          // Note: once an error occurs, remember to return after exiting the loop.
          if (!pendingJobID || *pendingJobID != jobID) {
              return;
          }
          auto result =
            utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(data);
          if (!result) {
              this->printer.printErr(result.error());
              loop.exit(-1);
              return;
          }
          // Note: should check return code of PackageManager1SearchResult
          auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
          if (resultCode != utils::error::ErrorCode::Success) {
              if (resultCode == utils::error::ErrorCode::Failed) {
                  this->printer.printErr(
                    LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
                  loop.exit(result->code);
                  return;
              }

              if (resultCode == utils::error::ErrorCode::NetworkError) {
                  this->printer.printMessage(_("Network connection failed. Please:"
                                               "\n1. Check your internet connection"
                                               "\n2. Verify network proxy settings if used"));
              }

              if (this->globalOptions.verbose) {
                  this->printer.printErr(
                    LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
              }

              loop.exit(result->code);
              return;
          }

          if (!result->packages) {
              this->printer.printPackages({});
              loop.exit(0);
              return;
          }

          auto allPackages = std::move(result->packages).value();
          if (!options.showDevel) {
              std::for_each(allPackages.begin(),
                            allPackages.end(),
                            [](decltype(allPackages)::reference pkgs) {
                                auto &vec = pkgs.second;

                                auto it =
                                  std::remove_if(vec.begin(),
                                                 vec.end(),
                                                 [](const api::types::v1::PackageInfoV2 &pkg) {
                                                     return pkg.packageInfoV2Module == "develop";
                                                 });
                                vec.erase(it, vec.end());
                            });
          }

          if (!options.type.empty()) {
              filterPackageInfosByType(allPackages, options.type);
          }

          // default only the latest version is displayed
          if (!options.showAllVersion) {
              filterPackageInfosByVersion(allPackages);
          }

          this->printer.printSearchResult(allPackages);
          loop.exit(0);
      });

    auto pendingReply = this->pkgMan.Search(utils::serialize::toQVariantMap(params));
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    pendingJobID = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::prune()
{
    LINGLONG_TRACE("command prune");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    QEventLoop loop;
    QString jobIDReply = "";
    connect(&this->pkgMan,
            &api::dbus::v1::PackageManager::PruneFinished,
            [this, &loop, &jobIDReply](const QString &jobID, const QVariantMap &data) {
                LINGLONG_TRACE("process prune result");
                if (jobIDReply != jobID) {
                    return;
                }
                auto ret =
                  utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PruneResult>(
                    data);
                if (!ret) {
                    this->printer.printErr(ret.error());
                    loop.exit(-1);
                    return;
                }

                if (!ret->packages) {
                    this->printer.printErr(LINGLONG_ERRV("No packages to prune."));
                    loop.exit(0);
                    return;
                }

                this->printer.printPruneResult(*ret->packages);
                loop.exit(0);
            });

    auto pendingReply = this->pkgMan.Prune();
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }
    jobIDReply = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::uninstall(const UninstallOptions &options)
{
    LINGLONG_TRACE("command uninstall");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto params = api::types::v1::PackageManager1UninstallParameters{};
    params.options = api::types::v1::CommonOptions{
        .force = options.forceOpt,
        .skipInteraction = false,
    };
    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }
    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
    }

    auto pendingReply = this->pkgMan.Uninstall(utils::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Uninstall);
    if (!res) {
        this->handleUninstallError(res.error());
        return -1;
    }

    waitTaskDone();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::list(const ListOptions &options)
{
    if (options.showUpgradeList) {
        auto upgradeList = this->listUpgradable();
        if (!upgradeList) {
            this->printer.printErr(upgradeList.error());
            return -1;
        }
        // 按id排序
        std::sort(upgradeList->begin(), upgradeList->end(), [](const auto &lhs, const auto &rhs) {
            return lhs.id < rhs.id;
        });
        this->printer.printUpgradeList(*upgradeList);
        return 0;
    }
    auto items = this->repository.listLayerItem();
    if (!items) {
        this->printer.printErr(items.error());
        return -1;
    }
    std::vector<api::types::v1::PackageInfoDisplay> list;
    for (const auto &item : *items) {
        nlohmann::json json = item.info;
        auto m = json.get<api::types::v1::PackageInfoDisplay>();
        auto t = this->repository.getLayerCreateTime(item);
        if (t.has_value()) {
            m.installTime = *t;
        }
        list.push_back(std::move(m));
    }
    if (!options.type.empty()) {
        filterPackageInfosByType(list, options.type);
    }
    // 按id排序
    std::sort(list.begin(), list.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.id < rhs.id;
    });
    this->printer.printPackages(list);
    return 0;
}

utils::error::Result<std::vector<api::types::v1::UpgradeListResult>> Cli::listUpgradable()
{
    LINGLONG_TRACE("list upgradable");

    // only applications can be upgraded
    auto upgradablePkgs = this->repository.upgradableApps();
    if (!upgradablePkgs) {
        return LINGLONG_ERR(upgradablePkgs);
    }

    std::vector<api::types::v1::UpgradeListResult> upgradeList;
    for (const auto &pkg : *upgradablePkgs) {
        upgradeList.emplace_back(
          api::types::v1::UpgradeListResult{ .id = pkg.first.id,
                                             .newVersion = pkg.second.reference.version.toString(),
                                             .oldVersion = pkg.first.version.toString() });
    }
    return upgradeList;
}

int Cli::repo(CLI::App *app, const RepoOptions &options)
{
    LINGLONG_TRACE("command repo");

    auto propCfg = this->pkgMan.configuration();
    if (this->pkgMan.lastError().isValid()) {
        auto err = LINGLONG_ERRV(this->pkgMan.lastError().message());
        this->printer.printErr(err);
        return -1;
    }

    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfigV2>(propCfg);
    if (!cfg) {
        qCritical() << cfg.error();
        qCritical() << "linglong bug detected.";
        std::abort();
    }

    auto argsParsed = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParsed("show")) {
        this->printer.printRepoConfig(*cfg);
        return 0;
    }

    if (argsParsed("modify")) {
        this->printer.printErr(
          LINGLONG_ERRV("sub-command 'modify' already has been deprecated, please use sub-command "
                        "'add' to add a remote repository and use it as default."));
        return EINVAL;
    }

    std::string url = options.repoUrl;

    if (argsParsed("add") || argsParsed("update")) {
        if (url.rfind("http", 0) != 0) {
            this->printer.printErr(LINGLONG_ERRV(QString{ "url is invalid: " } + url.c_str()));
            return EINVAL;
        }

        // remove last slash
        if (url.back() == '/') {
            url.pop_back();
        }
    }

    std::string name = options.repoName;
    // if alias is not set, use name as alias
    std::string alias = options.repoAlias.value_or(name);
    auto &cfgRef = *cfg;

    if (argsParsed("add")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return EINVAL;
        }

        bool isExist =
          std::any_of(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
              return repo.alias.value_or(repo.name) == alias;
          });
        if (isExist) {
            this->printer.printErr(
              LINGLONG_ERRV(QString{ "repo " } + alias.c_str() + " already exist."));
            return -1;
        }
        cfgRef.repos.push_back(api::types::v1::Repo{
          .alias = options.repoAlias,
          .name = name,
          .priority = 0,
          .url = url,
        });
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    auto existingRepo =
      std::find_if(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
          return repo.alias.value_or(repo.name) == alias;
      });

    if (existingRepo == cfgRef.repos.end()) {
        this->printer.printErr(
          LINGLONG_ERRV(QString{ "the operated repo " } + name.c_str() + " doesn't exist"));
        return -1;
    }

    if (argsParsed("remove")) {
        if (cfgRef.repos.size() == 1) {
            this->printer.printErr(LINGLONG_ERRV(QString{ "repo " } + alias.c_str()
                                                 + " is the only repo, please add another repo "
                                                   "before removing it or update it directly."));
            return -1;
        }
        cfgRef.repos.erase(existingRepo);

        if (cfgRef.defaultRepo == alias) {
            // choose the max priority repo as default repo
            auto maxPriority = linglong::repo::getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.priority == maxPriority) {
                    cfgRef.defaultRepo = repo.alias.value_or(repo.name);
                    break;
                }
            }
        }

        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("update")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return -1;
        }

        existingRepo->url = url;
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("enable-mirror")) {
        existingRepo->mirrorEnabled = true;
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("disable-mirror")) {
        existingRepo->mirrorEnabled = false;
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("set-default")) {
        if (cfgRef.defaultRepo != alias) {
            cfgRef.defaultRepo = alias;
            // set-default is equal to set-priority to the current max priority + 100
            auto maxPriority = linglong::repo::getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.alias.value_or(repo.name) == alias) {
                    repo.priority = maxPriority + 100;
                    break;
                }
            }
            return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
        }

        return 0;
    }

    if (argsParsed("set-priority")) {
        existingRepo->priority = options.repoPriority;
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    this->printer.printErr(LINGLONG_ERRV("unknown operation"));
    return -1;
}

int Cli::setRepoConfig(const QVariantMap &config)
{
    LINGLONG_TRACE("set repo config");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    this->pkgMan.setConfiguration(config);
    if (this->pkgMan.lastError().isValid()) {
        auto err = LINGLONG_ERRV(this->pkgMan.lastError().message());
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

int Cli::info(const InfoOptions &options)
{
    LINGLONG_TRACE("command info");

    QString app = QString::fromStdString(options.appid);

    if (app.isEmpty()) {
        auto err = LINGLONG_ERR("failed to get layer path").value();
        this->printer.printErr(err);
        return err.code();
    }

    QFileInfo file(app);
    auto isLayerFile = file.isFile() && file.suffix() == "layer";

    // 如果是app，显示app 包信息
    if (!isLayerFile) {
        auto fuzzyRef = package::FuzzyReference::parse(app.toStdString());
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto ref =
          this->repository.clearReference(*fuzzyRef,
                                          { .forceRemote = false, .fallbackToRemote = false });
        if (!ref) {
            qDebug() << ref.error();
            this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
            return -1;
        }

        auto layer = this->repository.getLayerDir(*ref, "binary");
        if (!layer) {
            this->printer.printErr(layer.error());
            return -1;
        }

        auto info = layer->info();
        if (!info) {
            this->printer.printErr(info.error());
            return -1;
        }
        this->printer.printPackage(*info);

        return 0;
    }

    // 如果是layer文件，显示layer文件 包信息
    const auto layerFile = package::LayerFile::New(app);

    if (!layerFile) {
        this->printer.printErr(layerFile.error());
        return -1;
    }

    const auto layerInfo = (*layerFile)->metaInfo();
    if (!layerInfo) {
        this->printer.printErr(layerInfo.error());
        return -1;
    }

    this->printer.printLayerInfo(*layerInfo);
    return 0;
}

int Cli::content(const ContentOptions &options)
{
    LINGLONG_TRACE("command content");

    QStringList contents{};

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               { .forceRemote = false, .fallbackToRemote = false });
    if (!ref) {
        qDebug() << ref.error();
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    auto layerItem = this->repository.getLayerItem(*ref);
    if (!layerItem) {
        this->printer.printErr(layerItem.error());
        return -1;
    }

    if (layerItem->info.kind != "app") {
        this->printer.printErr(LINGLONG_ERRV("Only supports viewing app content"));
        return -1;
    }

    auto layer = this->repository.getLayerDir(*ref, "binary");
    if (!layer) {
        this->printer.printErr(layer.error());
        return -1;
    }

    QDir entriesDir(layer->absoluteFilePath("entries/share"));
    if (!entriesDir.exists()) {
        this->printer.printErr(LINGLONG_ERR("no entries found").value());
        return -1;
    }

    QDirIterator it(entriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDot | QDir::NoDotDot | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        contents.append(it.fileInfo().absoluteFilePath());
    }
    // replace $LINGLONG_ROOT/layers/appid/verison/arch/module/entries to ${LINGLONG_ROOT}/entires
    contents.replaceInStrings(entriesDir.absolutePath(), QString(LINGLONG_ROOT) + "/entries/share");

    // only show the contents which are exported
    for (int pos = 0; pos < contents.size(); ++pos) {
        QFileInfo info(contents.at(pos));
        if (!info.exists()) {
            contents.removeAt(pos);
        }
    }

    this->printer.printContent(contents);
    return 0;
}

[[nodiscard]] std::string Cli::mappingFile(const std::filesystem::path &file) noexcept
{
    std::error_code ec;
    auto target = file;

    if (!target.is_absolute()) {
        return target;
    }

    if (std::filesystem::is_symlink(file, ec)) {
        std::array<char, PATH_MAX + 1> buf{};
        auto *real = ::realpath(file.c_str(), buf.data());

        if (real != nullptr) {
            target = real;
        } else {
            qCritical() << "resolve symlink " << file.c_str() << " error: " << ::strerror(errno);
        }
    }

    if (ec) {
        qCritical() << "failed to check symlink " << file.c_str() << ":" << ec.message().c_str();
    }

    // Dont't mapping the file under /home
    if (auto tmp = target.string(); tmp.rfind("/home/", 0) == 0) {
        return target;
    }

    return std::filesystem::path{ "/run/host/rootfs" }
    / std::filesystem::path{ target }.lexically_relative("/");
}

[[nodiscard]] std::string Cli::mappingUrl(std::string_view url) noexcept
{
    if (url.rfind('/', 0) == 0) {
        return mappingFile(url);
    }

    // if the scheme of url is "file", we need to map the native file path to the corresponding
    // container path, others will deliver to app directly.
    constexpr std::string_view filePrefix = "file://";
    if (url.rfind(filePrefix, 0) == 0) {
        std::filesystem::path nativePath = url.substr(filePrefix.size());
        std::filesystem::path target = mappingFile(nativePath);
        return std::string{ filePrefix } + target.string();
    }

    return std::string{ url };
}

std::vector<std::string> Cli::filePathMapping(const std::vector<std::string> &command,
                                              const RunOptions &options) const noexcept
{
    // FIXME: couldn't handel command like 'll-cli run org.xxx.yyy --file f1 f2 f3 org.xxx.yyy %%F'
    // can't distinguish the boundary of command , need validate the command arguments in the future

    std::vector<std::string> execArgs;
    // if the --file or --url option is specified, need to map the file path to the linglong
    // path(/run/host).
    for (const auto &arg : command) {
        if (arg.rfind('%', 0) != 0) {
            execArgs.emplace_back(arg);
            continue;
        }

        if (arg == "%f" || arg == "%F") {
            if (arg == "%f" && options.filePaths.size() > 1) {
                // refer:
                // https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html
                qWarning() << "more than one file path specified, all file paths will be passed.";
            }

            for (const auto &file : options.filePaths) {
                if (file.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingFile(file));
            }

            continue;
        }

        if (arg == "%u" || arg == "%U") {
            if (arg == "%u" && options.fileUrls.size() > 1) {
                qWarning() << "more than one url specified, all file paths will be passed.";
            }

            for (const auto &url : options.fileUrls) {
                if (url.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingUrl(url));
            }

            continue;
        }

        qWarning() << "unkown command argument" << QString::fromStdString(arg);
    }

    return execArgs;
}

void Cli::filterPackageInfosByType(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list,
  const std::string &type) noexcept
{
    // if type is all, do nothing, return app of all packages.
    if (type == "all") {
        return;
    }

    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> filtered;

    for (const auto &[key, packages] : list) {
        std::vector<api::types::v1::PackageInfoV2> filteredPackages;

        std::copy_if(packages.begin(),
                     packages.end(),
                     std::back_inserter(filteredPackages),
                     [&type](const api::types::v1::PackageInfoV2 &pkg) {
                         return pkg.kind == type;
                     });

        if (!filteredPackages.empty()) {
            filtered.emplace(key, std::move(filteredPackages));
        }
    }

    list = std::move(filtered);
}

void Cli::filterPackageInfosByType(std::vector<api::types::v1::PackageInfoDisplay> &list,
                                   const std::string &type)
{
    if (type == "all") {
        return;
    }
    list.erase(std::remove_if(list.begin(),
                              list.end(),
                              [&type](const api::types::v1::PackageInfoDisplay &pkg) {
                                  return pkg.kind != type;
                              }),
               list.end());
}

void Cli::filterPackageInfosByVersion(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list) noexcept
{
    for (const auto &[pkgRepo, packages] : list) {
        std::map<std::string, api::types::v1::PackageInfoV2> temp;
        for (const auto &pkgInfo : packages) {
            auto key =
              QString("%1-%2-%3")
                .arg(pkgRepo.c_str(), pkgInfo.id.c_str(), pkgInfo.packageInfoV2Module.c_str())
                .toStdString();

            auto it = temp.find(key);
            if (it == temp.end()) {
                temp.emplace(key, pkgInfo);
                continue;
            }

            auto oldVersion = package::Version::parse(it->second.version);
            if (!oldVersion) {
                qWarning() << "failed to parse old version:" << oldVersion.error().message();
                continue;
            }

            auto newVersion = package::Version::parse(pkgInfo.version);
            if (!newVersion) {
                qWarning() << "failed to parse new version:" << newVersion.error().message();
                continue;
            }

            if (*oldVersion < *newVersion) {
                it->second = pkgInfo;
            }
        }

        if (temp.empty()) {
            continue;
        }

        std::vector<api::types::v1::PackageInfoV2> filteredPackages;
        filteredPackages.reserve(temp.size());

        for (auto &[_, pkgInfo] : temp) {
            filteredPackages.emplace_back(std::move(pkgInfo));
        }

        auto it = list.find(pkgRepo);
        if (it != list.end()) {
            it->second = std::move(filteredPackages);
        } else {
            list.emplace(pkgRepo, std::move(filteredPackages));
        }
    }
}

utils::error::Result<void> Cli::ensureAuthorized()
{
    LINGLONG_TRACE("ensure authorized");

    auto authReply = this->authorization();
    if (!authReply.isValid()) {
        if (authReply.error().type() == QDBusError::AccessDenied) {
            auto ret = this->runningAsRoot();
            std::string message = "failed to authorize";
            if (!ret) {
                message += ": " + ret.error().message();
            }
            return LINGLONG_ERR(message);
        }

        return LINGLONG_ERR(authReply.error().message() + authReply.error().name(),
                            static_cast<int>(authReply.error().type()));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::runningAsRoot()
{
    return runningAsRoot(QCoreApplication::instance()->arguments());
}

utils::error::Result<void> Cli::runningAsRoot(const QList<QString> &args)
{
    LINGLONG_TRACE("run with pkexec");

    const char *pkexecBin = "pkexec";
    QStringList argv{ pkexecBin };
    argv.append(args);
    qDebug() << "run with pkexec:" << argv;
    std::vector<char *> targetArgv;
    for (const auto &arg : argv) {
        QByteArray byteArray = arg.toUtf8();
        targetArgv.push_back(strdup(byteArray.constData()));
    }
    targetArgv.push_back(nullptr);

    auto ret = execvp(pkexecBin, const_cast<char **>(targetArgv.data()));
    // NOTE: if reached here, exevpe is failed.
    for (auto arg : targetArgv) {
        free(arg);
    }
    return LINGLONG_ERR("execve error", ret);
}

QDBusReply<void> Cli::authorization()
{
    // Note: we have marked the method Permissions of PM as rejected.
    // Use this method to determin that this client whether have permission to call PM.
    return this->pkgMan.Permissions();
}

utils::error::Result<void>
Cli::RequestDirectories(const api::types::v1::PackageInfoV2 &info) noexcept
{
    LINGLONG_TRACE("request directories");

    // TODO: skip request directories for now
    return LINGLONG_OK;

    auto userHome = qgetenv("HOME").toStdString();
    if (userHome.empty()) {
        return LINGLONG_ERR("HOME is not set, skip request directories");
    }

    QDir dialogPath = QDir{ LINGLONG_LIBEXEC_DIR }.filePath("dialog");
    if (auto runtimeDir = qgetenv("LINGLONG_PERMISSION_DIALOG_DIR"); !runtimeDir.isEmpty()) {
        dialogPath.setPath(runtimeDir);
    }

    auto appDataDir = utils::xdg::appDataDir(info.id);
    if (!appDataDir) {
        return LINGLONG_ERR(appDataDir);
    }

    // make sure app data dir exists
    auto dir = QDir{ appDataDir->c_str() };
    if (!dir.mkpath(".")) {
        return LINGLONG_ERR(QString("make app data directory failed %1").arg(appDataDir->c_str()));
    }

    auto permissions = QDir{ appDataDir->c_str() }.absoluteFilePath("permissions.json");
    if (QFileInfo::exists(permissions)) {
        return LINGLONG_OK;
    }

    auto fd = ::shm_open(info.id.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return LINGLONG_ERR(QString{ "shm_open error:" } + ::strerror(errno));
    }
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    struct flock lock{
        .l_type = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = 0,
        .l_len = 0,
    };

    // all later processes should be blocked
    bool anotherRunning{ false };
    while (true) {
        using namespace std::chrono_literals;
        auto ret = ::fcntl(fd, F_SETLK, &lock);
        if (ret == -1) {
            if (errno == EACCES || errno == EAGAIN || errno == EINTR) {
                anotherRunning = true;
                std::this_thread::sleep_for(1s);
                continue;
            }

            return LINGLONG_ERR(QString{ "fcntl lock error: " } + ::strerror(errno));
        }

        if (!anotherRunning) {
            break;
        }

        lock.l_type = F_UNLCK;
        ret = ::fcntl(fd, F_SETLK, &lock);
        if (ret == -1) {
            return LINGLONG_ERR(QString{ "fcntl unlock error: " } + ::strerror(errno));
        }

        return LINGLONG_OK;
    }

    // unlink by creator
    auto releaseResource = utils::finally::finally([&info, &lock, fd] {
        lock.l_type = F_UNLCK;
        if (::fcntl(fd, F_SETLK, &lock) == -1) {
            qDebug() << "failed to unlock mem file:" << ::strerror(errno);
        }

        if (::shm_unlink(info.id.c_str()) == -1) {
            qDebug() << "shm_unlink error:" << ::strerror(errno);
        }
    });

    std::vector<api::types::v1::XdgDirectoryPermission> requiredDirs = {
        { .allowed = true, .dirType = "Desktop" },   { .allowed = true, .dirType = "Documents" },
        { .allowed = true, .dirType = "Downloads" }, { .allowed = true, .dirType = "Music" },
        { .allowed = true, .dirType = "Pictures" },  { .allowed = true, .dirType = "Videos" }
    };
    if (info.permissions && info.permissions->xdgDirectories) {
        requiredDirs = info.permissions->xdgDirectories.value();
    }

    if (requiredDirs.empty()) {
        qDebug() << "no required directories.";
        return LINGLONG_OK;
    }

    auto availableDialogs =
      dialogPath.entryInfoList(QDir::Executable | QDir::Files | QDir::NoSymLinks, QDir::Name);
    if (availableDialogs.empty()) {
        return LINGLONG_ERR("no available dialog");
    }

    auto dialogBin = availableDialogs.first().absoluteFilePath();
    QProcess dialogProc;
    dialogProc.setProgram(dialogBin);
    dialogProc.start();
    if (!dialogProc.waitForReadyRead(1000)) {
        dialogProc.kill();
        return LINGLONG_ERR("wait for reading from dialog " + dialogBin
                            + "failed:" + dialogProc.errorString());
    }

    auto rawData = dialogProc.read(4);
    auto *len = reinterpret_cast<uint32_t *>(rawData.data());
    rawData = dialogProc.read(*len);
    auto version = utils::serialize::LoadJSON<api::types::v1::DialogMessage>(rawData.data());
    if (!version) {
        dialogProc.kill();
        return LINGLONG_ERR("error reply from dialog:" + version.error().message());
    }

    if (version->type != "Handshake") {
        dialogProc.kill();
        return LINGLONG_ERR("dialog message type is not Handshake");
    }

    auto handshake =
      utils::serialize::LoadJSON<api::types::v1::DialogHandShakePayload>(version->payload);
    if (!handshake) {
        dialogProc.kill();
        return LINGLONG_ERR(" handshake message error:" + handshake.error().message());
    }

    if (handshake->version != "1.0") {
        dialogProc.kill();
        return LINGLONG_ERR(
          QString{ "incompatible version of dialog message protocol : required 1.0, actual " }
          + handshake->version.c_str());
    }

    auto request = api::types::v1::ApplicationPermissionsRequest{ .appID = info.id,
                                                                  .xdgDirectories = requiredDirs };
    api::types::v1::DialogMessage msg{ .payload = nlohmann::json(request).dump(),
                                       .type = "Request" };
    auto data = nlohmann::json(msg).dump();
    uint32_t size = data.size();
    rawData = QByteArray{ reinterpret_cast<char *>(&size), 4 };
    rawData.append(data.c_str());

    dialogProc.write(rawData);
    dialogProc.closeWriteChannel();
    if (!dialogProc.waitForFinished(16 * 1000)) {
        qWarning() << dialogProc.readAllStandardError();
        dialogProc.kill();
        return LINGLONG_ERR("dialog timeout");
    }

    bool allowRequired{ true };
    if (dialogProc.exitCode() != 0) {
        qDebug() << "dialog exited with code" << dialogProc.exitCode() << ":"
                 << dialogProc.readAllStandardError();
        allowRequired = false;
    }

    std::for_each(requiredDirs.begin(),
                  requiredDirs.end(),
                  [allowRequired](api::types::v1::XdgDirectoryPermission &dir) {
                      dir.allowed = allowRequired;
                  });
    api::types::v1::ApplicationConfigurationPermissions privs{ .xdgDirectories = requiredDirs };
    auto content = QByteArray::fromStdString(nlohmann::json(privs).dump());

    QFile permissionFile{ permissions };
    if (!permissionFile.open(QIODevice::WriteOnly | QIODevice::NewOnly | QIODevice::Text)) {
        return LINGLONG_ERR(permissionFile);
    }

    if (permissionFile.write(content) != content.size()) {
        qWarning() << "incomplete write to" << permissions << ":" << permissionFile.errorString();
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::generateLDCache(runtime::RunContext &runContext,
                                                const std::string &ldConf) noexcept
{
    LINGLONG_TRACE("generate ld cache");

    auto appLayerItem = runContext.getCachedAppItem();
    if (!appLayerItem) {
        return LINGLONG_ERR(appLayerItem);
    }

    auto appLayer = runContext.getAppLayer();
    if (!appLayer) {
        return LINGLONG_ERR("app layer not found");
    }
    auto appRef = appLayer->getReference();

    auto appCache = common::dir::getUserCacheDir() / appLayerItem->commit;
    std::error_code ec;
    std::filesystem::create_directories(appCache, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to create cache directory {}: ", appCache), ec);
    }

    generator::ContainerCfgBuilder cfgBuilder;
    auto res = runContext.fillContextCfg(cfgBuilder, ".ldcache");
    if (!res) {
        return LINGLONG_ERR(res);
    }

    auto uid = getuid();
    auto gid = getgid();

    std::filesystem::path ldConfPath{ appCache / "ld.so.conf" };

    cfgBuilder.setAppId(appRef.id)
      .setAppCache(appCache, false)
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindDefault()
      .bindCgroup()
      .bindXDGRuntime()
      .bindUserGroup()
      .forwardDefaultEnv()
      .addExtraMounts(
        std::vector<ocppi::runtime::config::types::Mount>{ ocppi::runtime::config::types::Mount{
          .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
          .options = { { "rbind", "ro" } },
          .source = ldConfPath,
          .type = "bind",
        } })
      .enableSelfAdjustingMount();

    // generate ld config
    {
        std::ofstream ofs(ldConfPath, std::ios::binary | std::ios::out | std::ios::trunc);
        Q_ASSERT(ofs.is_open());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("create ld config in bundle directory");
        }
        ofs << ldConf;
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process process{};
    process.cwd = "/";
    process.noNewPrivileges = true;
    process.terminal = true;
    process.args =
      std::vector<std::string>{ "/sbin/ldconfig", "-X", "-C", "/run/linglong/cache/ld.so.cache" };

    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<std::filesystem::path> Cli::ensureCache(
  runtime::RunContext &runContext, const generator::ContainerCfgBuilder &cfgBuilder) noexcept
{
    LINGLONG_TRACE("ensure cache");

    auto appLayerItem = runContext.getCachedAppItem();
    if (!appLayerItem) {
        return LINGLONG_ERR(appLayerItem);
    }

    auto appLayer = runContext.getAppLayer();
    if (!appLayer) {
        return LINGLONG_ERR("app layer not found");
    }
    auto appRef = appLayer->getReference();

    auto appCache = common::dir::getUserCacheDir() / appLayerItem->commit;
    bool ldCacheGen = true;
    auto ldConf = cfgBuilder.ldConf(appRef.arch.getTriplet());

    do {
        std::error_code ec;
        if (!std::filesystem::exists(appCache, ec)) {
            break;
        }

        // check ld.so.conf
        {
            auto ldSoConf = appCache / "ld.so.conf";
            if (!std::filesystem::exists(ldSoConf, ec)
                || !std::filesystem::exists(appCache / "ld.so.cache")) {
                break;
            }

            // If the ld.so.conf exists, check if it is consistent with the current configuration.
            std::stringstream oldCache;
            std::ifstream ifs(ldSoConf, std::ios::binary | std::ios::in);
            if (!ifs.is_open()) {
                return LINGLONG_ERR("failed to open " + QString::fromStdString(ldSoConf.string()));
            }
            oldCache << ifs.rdbuf();
            qDebug() << "ld.so.conf:" << QString::fromStdString(ldConf);
            qDebug() << "old ld.so.conf:" << QString::fromStdString(oldCache.str());
            if (oldCache.str() != ldConf) {
                break;
            }

            ldCacheGen = false;
        }
    } while (false);

    if (ldCacheGen) {
        auto res = generateLDCache(runContext, ldConf);
        if (!res) {
            return LINGLONG_ERR("failed to generate ld cache", res);
        }
    }

    return appCache;
}

void Cli::updateAM() noexcept
{
    // NOTE: make sure AM refresh the cache of desktop
    if ((QSysInfo::productType() == "Deepin" || QSysInfo::productType() == "deepin")
        && this->taskState.state == linglong::api::types::v1::State::Succeed) {
        QDBusConnection conn = QDBusConnection::systemBus();
        if (!conn.isConnected()) {
            qWarning() << "Failed to connect to the system bus";
        }

        auto peer = linglong::api::dbus::v1::DBusPeer("org.desktopspec.ApplicationUpdateNotifier1",
                                                      "/org/desktopspec/ApplicationUpdateNotifier1",
                                                      conn);
        auto reply = peer.Ping();
        reply.waitForFinished();
        if (!reply.isValid()) {
            qWarning() << "Failed to ping org.desktopspec.ApplicationUpdateNotifier1"
                       << reply.error();
        }
    }
}

int Cli::inspect(CLI::App *app, const InspectOptions &options)
{
    LINGLONG_TRACE("command inspect");

    auto argsParseFunc = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParseFunc("dir")) {
        if (options.dirType == "layer") {
            return this->getLayerDir(options);
        } else if (options.dirType == "bundle") {
            return this->getBundleDir(options);
        } else {
            this->printer.printErr(
              LINGLONG_ERRV(QString("Invalid type: %1, type must be layer or bundle")
                              .arg(QString::fromStdString(options.dirType))));
            return -1;
        }
    }

    return 0;
}

int Cli::getLayerDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Layer dir");

    auto fuzzyString = options.appid;

    auto fuzzyRef = package::FuzzyReference::parse(fuzzyString);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               { .forceRemote = false, .fallbackToRemote = false });
    if (!ref) {
        qDebug() << ref.error();
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    std::string module = "binary";
    if (!options.module.empty()) {
        module = options.module;
    }

    auto layerDir = this->repository.getLayerDir(*ref, module);
    if (!layerDir) {
        this->printer.printErr(layerDir.error());
        return -1;
    }

    std::cout << layerDir->absolutePath().toStdString() << std::endl;

    return 0;
}

int Cli::getBundleDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Bundle dir");

    auto containerIDList = getRunningAppContainers(options.appid);

    if (containerIDList.empty()) {
        this->printer.printErr(LINGLONG_ERRV("Can not find the running application."));
        return -1;
    }

    if (containerIDList.size() > 1) {
        this->printer.printErr(
          LINGLONG_ERRV("Found multiple running containers for the application, please specify "
                        "the container ID to inspect."));
        return -1;
    }

    auto bundleDir = linglong::common::dir::getBundleDir(containerIDList.front());

    std::cout << bundleDir.string() << std::endl;

    return 0;
}

utils::error::Result<void> Cli::initInteraction()
{
    LINGLONG_TRACE("initInteraction");

    auto conn = this->pkgMan.connection();
    auto con = conn.connect(this->pkgMan.service(),
                            this->pkgMan.path(),
                            this->pkgMan.interface(),
                            "RequestInteraction",
                            this,
                            SLOT(interaction(QDBusObjectPath, int, QVariantMap)));
    if (!con) {
        return LINGLONG_ERR("Failed to connect signal: RequestInteraction");
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::waitTaskCreated(QDBusPendingReply<QVariantMap> &reply,
                                                TaskType taskType)
{
    LINGLONG_TRACE("waitTaskCreated");

    auto result = waitDBusReply<api::types::v1::PackageManager1PackageTaskResult>(reply);
    if (!result) {
        return LINGLONG_ERR(result.error());
    }

    auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
    if (resultCode != utils::error::ErrorCode::Success) {
        return LINGLONG_ERR(result->message, result->code);
    }

    auto conn = pkgMan.connection();
    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    this->task = new api::dbus::v1::Task1(pkgMan.service(), taskObjectPath, conn);
    this->taskState.state = linglong::api::types::v1::State::Queued;
    this->taskState.taskType = taskType;

    LogD("task object path: {}", this->taskObjectPath);

    if (!conn.connect(pkgMan.service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        Q_ASSERT(false);
        return LINGLONG_ERR(fmt::format("Failed to connect signal PropertiesChanged: {}",
                                        conn.lastError().message()));
    }

    return LINGLONG_OK;
}

void Cli::waitTaskDone()
{
    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        LogE("connect taskDone failed");
        return;
    }
    loop.exec();
}

void Cli::handleInstallError(const utils::error::Error &error,
                             const api::types::v1::PackageManager1InstallParameters &params)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppInstallModuleRequireAppFirst:
        this->printer.printMessage(_("To install the module, one must first install the app."));
        break;
    case utils::error::ErrorCode::AppInstallModuleAlreadyExists:
        this->printer.printMessage(_("Module is already installed."));
        break;
    case utils::error::ErrorCode::AppInstallModuleNotFound:
        this->printer.printMessage(_("The module could not be found remotely."));
        break;
    case utils::error::ErrorCode::AppInstallAlreadyInstalled:
        this->printer.printMessage(
          fmt::format(_("Application already installed, If you want to replace it, try using "
                        "'ll-cli install {} --force'"),
                      params.package.id));
        break;
    case utils::error::ErrorCode::AppInstallNotFoundFromRemote:
        this->printer.printMessage(
          fmt::format(_("Application {} is not found in remote repo."), params.package.id));
        break;
    case utils::error::ErrorCode::AppInstallModuleNoVersion:
        this->printer.printMessage(_("Cannot specify a version when installing a module."));
        break;
    case utils::error::ErrorCode::AppInstallNeedDowngrade:
        this->printer.printMessage(
          fmt::format(_("The latest version has been installed. If you want to "
                        "replace it, try using 'll-cli install {} --force'"),
                      params.package.id));
        break;
    case utils::error::ErrorCode::Unknown:
    case utils::error::ErrorCode::AppInstallFailed:
        this->printer.printMessage(_("Install failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUninstallError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUninstallAppIsRunning: {

        auto ret = this->notifier->notify(api::types::v1::InteractionRequest{
          .appName = "ll-cli",
          .summary = _("The application is currently running and cannot be "
                       "uninstalled. Please turn off the application and try again.") });
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        break;
    }
    case utils::error::ErrorCode::AppUninstallNotFoundFromLocal:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUninstallMultipleVersions:
        this->printer.printMessage(
          fmt::format(_("Multiple versions of the package are installed. Please specify a single "
                        "version to uninstall:\n{}"),
                      error.message()));
        break;
    case utils::error::ErrorCode::AppUninstallBaseOrRuntime:
        this->printer.printMessage(
          _("Base or runtime cannot be uninstalled, please use 'll-cli prune'."));
        break;
    case utils::error::ErrorCode::AppUninstallFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Uninstall failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUpgradeError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUpgradeLocalNotFound:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUpgradeFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Upgrade failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

bool Cli::handleCommonError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::NetworkError:
        this->printer.printMessage(_("Network connection failed. Please:"
                                     "\n1. Check your internet connection"
                                     "\n2. Verify network proxy settings if used"));
        break;
    case utils::error::ErrorCode::LayerCompatibilityError:
        this->printer.printMessage(_("Package not found"));
        break;
    case utils::error::ErrorCode::Canceled:
        this->printer.printMessage(_("Operation canceled"));
        break;
    default:
        this->printer.printErr(error);
        return false;
    }

    return true;
}

void Cli::detectDrivers()
{
    QProcess process;
    process.setProgram(QString(LINGLONG_LIBEXEC_DIR "/ll-driver-detect"));
    // 禁用标准输入 (stdin)
    process.setStandardInputFile("/dev/null");
    // 禁用标准输出 (stdout)
    process.setStandardOutputFile("/dev/null");
    // 禁用标准错误输出 (stderr)
    process.setStandardErrorFile("/dev/null");
    process.startDetached();
}

} // namespace linglong::cli
