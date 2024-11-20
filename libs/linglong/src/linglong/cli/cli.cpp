/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1PackageTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/api/types/v1/SubState.hpp"
#include "linglong/api/types/v1/UpgradeListResult.hpp"
#include "linglong/cli/printer.h"
#include "linglong/package/layer_file.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <nlohmann/json.hpp>

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFileInfo>

#include <filesystem>
#include <iostream>
#include <system_error>

#include <fcntl.h>

using namespace linglong::utils::error;

namespace linglong::cli {

void Cli::onTaskPropertiesChanged(QString interface,                                   // NOLINT
                                  QVariantMap changed_properties,                      // NOLINT
                                  [[maybe_unused]] QStringList invalidated_properties) // NOLINT
{
    LINGLONG_TRACE("update task properties")

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

            lastState = static_cast<api::types::v1::State>(val);
            continue;
        }

        if (key == "SubState") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                qCritical() << "dbus ipc error, SubState couldn't convert to int";
                continue;
            }

            lastSubState = static_cast<api::types::v1::SubState>(val);
            continue;
        }

        if (key == "Percentage") {
            bool ok{ false };
            auto val = value.toDouble(&ok);
            if (!ok) {
                qCritical() << "dbus ipc error, Percentage couldn't convert to int";
                continue;
            }

            lastPercentage = val;
            continue;
        }

        if (key == "Message") {
            if (!value.canConvert<QString>()) {
                qCritical() << "dbus ipc error, Message couldn't convert to QString";
                continue;
            }

            lastMessage = value.toString();
            continue;
        }
    }

    printProgress();
}

void Cli::interaction(QDBusObjectPath object_path, int messageID, QVariantMap additionalMessage)
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
        auto tips = QString("The lower version %1 is currently installed. Do you "
                            "want to continue installing the latest version %2?")
                      .arg(QString::fromStdString(msg->localRef))
                      .arg(QString::fromStdString(msg->remoteRef));
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
        if (dbusReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return;
        }

        this->printer.printErr(
          LINGLONG_ERRV(dbusReply.error().message(), dbusReply.error().type()));
    }
}

void Cli::onTaskAdded([[maybe_unused]] QDBusObjectPath object_path)
{
    qDebug() << "task added" << object_path.path();
}

void Cli::onTaskRemoved(
  QDBusObjectPath object_path, int state, int subState, QString message, double percentage)
{
    if (object_path.path() != taskObjectPath) {
        return;
    }

    delete task;
    task = nullptr;

    // no change, skip
    if (lastState == static_cast<api::types::v1::State>(state)
        && lastSubState == static_cast<api::types::v1::SubState>(subState) && lastMessage == message
        && lastPercentage == percentage) {
        Q_EMIT taskDone();
        return;
    }

    this->lastState = static_cast<api::types::v1::State>(state);
    this->lastSubState = static_cast<api::types::v1::SubState>(subState);
    this->lastMessage = std::move(message);
    this->lastPercentage = percentage;

    if (this->lastSubState == api::types::v1::SubState::AllDone) {
        this->printProgress();
    } else if (this->lastSubState == api::types::v1::SubState::PackageManagerDone) {
        this->notifier->notify(
          api::types::v1::InteractionRequest{ .summary = this->lastMessage.toStdString() });
    }

    Q_EMIT taskDone();
}

void Cli::printProgress() noexcept
{
    LINGLONG_TRACE("print progress")
    if (this->lastState == api::types::v1::State::Unknown) {
        qInfo() << "task is invalid";
        return;
    }

    this->printer.printTaskState(this->lastPercentage,
                                 this->lastMessage,
                                 this->lastState,
                                 this->lastSubState);
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
        qFatal("couldn't connect to package manager signal 'TaskAdded'");
    }

    if (!conn.connect(pkgMan.service(),
                      pkgMan.path(),
                      pkgMan.interface(),
                      "TaskRemoved",
                      this,
                      SLOT(onTaskRemoved(QDBusObjectPath, int, int, QString, double)))) {
        qFatal("couldn't connect to package manager signal 'TaskRemoved'");
    }
}

int Cli::run()
{
    LINGLONG_TRACE("command run");
    // NOTE: ll-box is not support running as root for now.
    if (getuid() == 0) {
        qInfo() << "'ll-cli run' currently does not support running as root.";
        return -1;
    }

    auto userContainerDir = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());
    auto mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    auto pidFile = userContainerDir / std::to_string(::getpid());
    auto fd = ::open(pidFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd == -1) {
        qCritical() << QString{ "create file " } + pidFile.c_str() + " error:" + ::strerror(errno);
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

    const auto userInputAPP = QString::fromStdString(options.appid);
    Q_ASSERT(!userInputAPP.isEmpty());

    auto fuzzyRef = package::FuzzyReference::parse(userInputAPP);
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
    auto appLayerDir = this->repository.getMergedModuleDir(*curAppRef);
    if (!appLayerDir) {
        this->printer.printErr(appLayerDir.error());
        return -1;
    }

    auto info = appLayerDir->info();
    if (!info) {
        this->printer.printErr(info.error());
        return -1;
    }

    std::optional<std::string> runtimeLayerRef;
    std::optional<package::LayerDir> runtimeLayerDir;
    if (info->runtime) {
        auto runtimeFuzzyRef =
          package::FuzzyReference::parse(QString::fromStdString(*info->runtime));
        if (!runtimeFuzzyRef) {
            this->printer.printErr(runtimeFuzzyRef.error());
            return -1;
        }

        auto runtimeRefRet = this->repository.clearReference(*runtimeFuzzyRef,
                                                             {
                                                               .forceRemote = false,
                                                               .fallbackToRemote = false,
                                                             });
        if (!runtimeRefRet) {
            this->printer.printErr(runtimeRefRet.error());
            return -1;
        }
        auto &runtimeRef = *runtimeRefRet;

        if (!info->uuid.has_value()) {
            auto runtimeLayerDirRet = this->repository.getMergedModuleDir(runtimeRef);
            if (!runtimeLayerDirRet) {
                this->printer.printErr(runtimeLayerDirRet.error());
                return -1;
            }
            runtimeLayerDir = *runtimeLayerDirRet;
        } else {
            auto runtimeLayerDirRet =
              this->repository.getLayerDir(*runtimeRefRet, std::string{ "binary" }, info->uuid);
            if (!runtimeLayerDirRet) {
                this->printer.printErr(runtimeLayerDirRet.error());
                return -1;
            }
            runtimeLayerRef = runtimeRefRet->toString().toStdString();
            runtimeLayerDir = std::move(runtimeLayerDirRet).value();
        }
    }

    auto baseFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(info->base));
    if (!baseFuzzyRef) {
        this->printer.printErr(baseFuzzyRef.error());
        return -1;
    }

    auto baseRef = this->repository.clearReference(*baseFuzzyRef,
                                                   {
                                                     .forceRemote = false,
                                                     .fallbackToRemote = false,
                                                   });
    if (!baseRef) {
        this->printer.printErr(LINGLONG_ERRV(baseRef));
        return -1;
    }
    utils::error::Result<package::LayerDir> baseLayerDir;
    if (!info->uuid.has_value()) {
        qDebug() << "getMergedModuleDir base";
        baseLayerDir = this->repository.getMergedModuleDir(*baseRef);
    } else {
        qDebug() << "getLayerDir base" << info->uuid.value().c_str();
        baseLayerDir = this->repository.getLayerDir(*baseRef, std::string{ "binary" }, info->uuid);
    }
    if (!baseLayerDir) {
        this->printer.printErr(LINGLONG_ERRV(baseLayerDir));
        return -1;
    }

    auto commands = options.commands;
    if (commands.empty()) {
        commands = info->command.value_or(std::vector<std::string>{});
    }

    if (commands.empty()) {
        qWarning() << "invalid command found in package" << QString::fromStdString(info->id);
        commands = { "bash" };
    }
    auto execArgs = filePathMapping(commands);

    auto newContainerID = [id = curAppRef->toString() + "-"]() mutable {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        id.append(QByteArray::fromStdString(std::to_string(now)));
        return QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex();
    }();

    // this lambda will dump reference of containerID, app, base and runtime to
    // /run/linglong/getuid()/getpid() to store these needed infomation
    auto dumpContainerInfo = [app = curAppRef->toString().toStdString(),
                              base = baseRef->toString().toStdString(),
                              containerID = newContainerID.toStdString(),
                              runtime = runtimeLayerRef,
                              this]() -> bool {
        LINGLONG_TRACE("dump info")
        std::error_code ec;
        auto pidFile = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid())
          / std::to_string(::getpid());
        if (!std::filesystem::exists(pidFile, ec)) {
            if (ec) {
                qCritical() << "couldn't get status of" << pidFile.c_str() << ":"
                            << ec.message().c_str();
                return ec.value();
            }

            auto msg = "state file " + pidFile.string() + "doesn't exist, abort.";
            qFatal("%s", msg.c_str());
        }

        auto stateInfo = linglong::api::types::v1::ContainerProcessStateInfo{
            .app = app,
            .base = base,
            .containerID = containerID,
            .runtime = runtime,
        };

        std::ofstream stream{ pidFile };
        if (!stream.is_open()) {
            auto msg = QString{ "failed to open " } + pidFile.c_str();
            this->printer.printErr(LINGLONG_ERRV(msg));
            return false;
        }
        stream << nlohmann::json(stateInfo).dump();
        stream.close();

        return true;
    };

    auto containers = getCurrentContainers().value_or(std::vector<api::types::v1::CliContainer>{});
    for (const auto &container : containers) {
        if (container.package != curAppRef->toString().toStdString()) {
            qDebug() << "mismatch:" << container.package.c_str() << " -- " << curAppRef->toString();
            continue;
        }

        if (!dumpContainerInfo()) {
            return -1;
        }

        QStringList bashArgs;
        // 为避免原始args包含空格，每个arg都使用单引号包裹，并对arg内部的单引号进行转义替换
        for (const auto &arg : execArgs) {
            bashArgs.push_back(
              QString("'%1'").arg(QString::fromStdString(arg).replace("'", "'\\''")));
        }

        if (!bashArgs.isEmpty()) {
            // exec命令使用原始args中的进程替换bash进程
            bashArgs.prepend("exec");
        }
        // 在原始args前面添加bash --login -c，这样可以使用/etc/profile配置的环境变量
        execArgs = std::vector<std::string>{ "/bin/bash",
                                             "--login",
                                             "-c",
                                             bashArgs.join(" ").toStdString(),
                                             "; wait" };

        auto opt = ocppi::runtime::ExecOption{};
        opt.uid = ::getuid();
        opt.gid = ::getgid();

        auto result = this->ociCLI.exec(container.id,
                                        execArgs[0],
                                        { execArgs.cbegin() + 1, execArgs.cend() },
                                        opt);

        if (!result) {
            auto err = LINGLONG_ERRV(result);
            this->printer.printErr(err);
            return -1;
        }

        return 0;
    }

    std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
    auto bindMount =
      [&applicationMounts](const api::types::v1::ApplicationConfigurationPermissionsBind &bind) {
          applicationMounts.push_back(ocppi::runtime::config::types::Mount{
            .destination = bind.destination,
            .gidMappings = {},
            .options = { { "rbind" } },
            .source = bind.source,
            .type = "bind",
            .uidMappings = {},
          });
      };

    auto bindInnerMount =
      [&applicationMounts](
        const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
          applicationMounts.push_back(ocppi::runtime::config::types::Mount{
            .destination = bind.destination,
            .gidMappings = {},
            .options = { { "rbind" } },
            .source = "rootfs" + bind.source,
            .type = "bind",
            .uidMappings = {},
          });
      };

    if (info->permissions) {
        auto &perm = info->permissions;
        if (perm->binds) {
            const auto &binds = perm->binds;
            std::for_each(binds->cbegin(), binds->cend(), bindMount);
        }

        if (perm->innerBinds) {
            const auto &innerBinds = perm->innerBinds;
            const auto &hostSourceDir =
              std::filesystem::path{ appLayerDir->absolutePath().toStdString() };
            std::for_each(innerBinds->cbegin(), innerBinds->cend(), bindInnerMount);
        }
    }

    auto container = this->containerBuilder.create({
      .appID = curAppRef->id,
      .containerID = newContainerID,
      .runtimeDir = runtimeLayerDir,
      .baseDir = *baseLayerDir,
      .appDir = *appLayerDir,
      .patches = {},
      .mounts = std::move(applicationMounts),
      .masks = {},
    });
    if (!container) {
        this->printer.printErr(container.error());
        return -1;
    }

    ocppi::runtime::config::types::Process process{};
    process.args = execArgs;

    if (!dumpContainerInfo()) {
        return -1;
    }

    auto result = (*container)->run(process);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    return 0;
}

int Cli::exec()
{
    LINGLONG_TRACE("ll-cli exec");

    auto containers = getCurrentContainers();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    std::string containerID;
    auto instance = options.instance;
    for (const auto &container : *containers) {
        if (container.package == instance) {
            containerID = container.id;
            break;
        }
    }

    if (containerID.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    qInfo() << "select container id" << QString::fromStdString(containerID);

    auto commands = options.commands;
    if (commands.size() != 0) {
        QStringList bashArgs;
        // 为避免原始args包含空格，每个arg都使用单引号包裹，并对arg内部的单引号进行转义替换
        for (const auto &arg : commands) {
            bashArgs.push_back(
              QString("'%1'").arg(QString::fromStdString(arg).replace("'", "'\\''")));
        }

        if (!bashArgs.isEmpty()) {
            // exec命令使用原始args中的进程替换bash进程
            bashArgs.prepend("exec");
        }
        // 在原始args前面添加bash --login -c，这样可以使用/etc/profile配置的环境变量
        commands = std::vector<std::string>{ "/bin/bash",
                                             "--login",
                                             "-c",
                                             bashArgs.join(" ").toStdString(),
                                             "; wait" };
    } else {
        commands = { "bash", "--login" };
    }

    auto opt = ocppi::runtime::ExecOption{};
    opt.uid = ::getuid();
    opt.gid = ::getgid();

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
    for (const auto &pidFile : std::filesystem::directory_iterator{ infoDir }) {
        const auto &file = pidFile.path();
        const auto &process = "/proc" / file.filename();

        std::error_code ec;
        if (!std::filesystem::exists(process, ec)) {
            // this process may exit abnormally, skip it.
            qDebug() << process.c_str() << "doesn't exist";
            continue;
        }

        auto info = linglong::utils::serialize::LoadJSONFile<
          linglong::api::types::v1::ContainerProcessStateInfo>(
          QString::fromStdString(file.string()));
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
    LINGLONG_TRACE("command ps");

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

int Cli::kill()
{
    LINGLONG_TRACE("command kill");

    auto containers = getCurrentContainers();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    std::string containerID;
    auto instance = options.instance;
    for (const auto &container : *containers) {
        if (container.package == instance) {
            containerID = container.id;
            break;
        }
    }

    if (containerID.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    qInfo() << "select container id" << QString::fromStdString(containerID);

    auto result = this->ociCLI.kill(ocppi::runtime::ContainerID(containerID),
                                    ocppi::runtime::Signal("SIGTERM"));
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }

    return 0;
}

void Cli::cancelCurrentTask()
{
    bool isRunning = this->lastSubState != linglong::api::types::v1::SubState::AllDone
      && this->lastSubState != linglong::api::types::v1::SubState::PackageManagerDone;
    if (isRunning && this->task != nullptr) {
        this->task->Cancel();
        std::cout << "cancel running task." << std::endl;
    }
}

int Cli::installFromFile(const QFileInfo &fileInfo, const api::types::v1::CommonOptions &options)
{
    auto filePath = fileInfo.absoluteFilePath();
    LINGLONG_TRACE(QString{ "install from file %1" }.arg(filePath))

    qInfo() << "install from file" << filePath;
    QFile file{ filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        auto err = LINGLONG_ERR(file);
        this->printer.printErr(err.value());
        return -1;
    }

    auto conn = this->pkgMan.connection();
    auto con = conn.connect(this->pkgMan.service(),
                            this->pkgMan.path(),
                            this->pkgMan.interface(),
                            "RequestInteraction",
                            this,
                            SLOT(interaction(QDBusObjectPath, int, QVariantMap)));
    if (!con) {
        qCritical() << "Failed to connect signal: RequestInteraction. state may be incorrect.";
        return -1;
    }

    QDBusUnixFileDescriptor dbusFileDescriptor(file.handle());

    auto pendingReply = this->pkgMan.InstallFromFile(dbusFileDescriptor,
                                                     fileInfo.suffix(),
                                                     utils::serialize::toQVariantMap(options));
    pendingReply.waitForFinished();
    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }
        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(
        pendingReply.value());
    if (!result) {
        qCritical() << result.error();
        qCritical() << "linglong bug detected.";
        std::abort();
    }

    if (result->code != 0) {
        auto err = LINGLONG_ERRV(QString::fromStdString(result->message), result->code);
        this->printer.printErr(err);
        return -1;
    }

    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    task = new api::dbus::v1::Task1(pkgMan.service(), taskObjectPath, conn);

    if (!conn.connect(pkgMan.service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qCritical() << "connect PropertiesChanged failed:" << conn.lastError();
        Q_ASSERT(false);
        return -1;
    }

    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        qCritical() << "connect taskDone failed";
        return -1;
    }
    loop.exec();

    if (this->lastState != linglong::api::types::v1::State::Succeed) {
        return -1;
    }

    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::install()
{
    LINGLONG_TRACE("command install");

    // Note: we deny the org.freedesktop.DBus.Introspectable for now.
    // Use this interface to determin that this client whether have permission to call PM.
    QDBusInterface dbusIntrospect(this->pkgMan.service(),
                                  this->pkgMan.path(),
                                  "org.freedesktop.DBus.Introspectable",
                                  this->pkgMan.connection());
    QDBusReply<QString> authReply = dbusIntrospect.call("Introspect");
    if (!authReply.isValid() && authReply.error().type() == QDBusError::AccessDenied) {
        auto ret = this->runningAsRoot();
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        return -1;
    }

    auto app = QString::fromStdString(options.appid);
    QFileInfo info(app);

    auto params =
      api::types::v1::PackageManager1InstallParameters{ .options = { .force = false,
                                                                     .skipInteraction = false } };
    params.options.force = options.forceOpt;
    params.options.skipInteraction = options.confirmOpt;

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(QFileInfo{ info.absoluteFilePath() }, params.options);
    }

    auto conn = this->pkgMan.connection();
    auto con = conn.connect(this->pkgMan.service(),
                            this->pkgMan.path(),
                            this->pkgMan.interface(),
                            "RequestInteraction",
                            this,
                            SLOT(interaction(QDBusObjectPath, int, QVariantMap)));
    if (!con) {
        qCritical() << "Failed to connect signal: RequestInteraction. state may be incorrect.";
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(app);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    params.package.id = fuzzyRef->id.toStdString();
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel->toStdString();
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version->toString().toStdString();
    }

    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
    }

    auto pendingReply = this->pkgMan.Install(utils::serialize::toQVariantMap(params));
    pendingReply.waitForFinished();

    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        this->printer.printErr(
          LINGLONG_ERRV(pendingReply.error().message(), pendingReply.error().type()));
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(
        pendingReply.value());
    if (!result) {
        qCritical() << "bug detected:" << result.error().message();
        std::abort();
    }

    if (result->code != 0) {
        this->printer.printReply({ .code = result->code, .message = result->message });
        return -1;
    }

    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    task = new api::dbus::v1::Task1(pkgMan.service(), taskObjectPath, conn);

    if (!conn.connect(pkgMan.service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qCritical() << "connect PropertiesChanged failed:" << conn.lastError();
        Q_ASSERT(false);
        return -1;
    }

    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        qCritical() << "connect taskDone failed";
        return -1;
    }
    loop.exec();

    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::upgrade()
{
    LINGLONG_TRACE("command upgrade");

    QDBusInterface dbusIntrospect(this->pkgMan.service(),
                                  this->pkgMan.path(),
                                  "org.freedesktop.DBus.Introspectable",
                                  this->pkgMan.connection());
    QDBusReply<QString> authReply = dbusIntrospect.call("Introspect");
    if (!authReply.isValid() && authReply.error().type() == QDBusError::AccessDenied) {
        auto ret = this->runningAsRoot();
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        return -1;
    }

    std::vector<package::FuzzyReference> fuzzyRefs;
    if (!options.appid.empty()) {
        auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(options.appid));
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }
        fuzzyRefs.emplace_back(std::move(*fuzzyRef));
    } else {
        // Note: upgrade all apps for now.
        auto list = this->listUpgradable("app");
        if (!list) {
            this->printer.printErr(list.error());
            return -1;
        }
        for (const auto &item : *list) {
            auto fuzzyRef =
              package::FuzzyReference::parse(QString("%1/%2")
                                               .arg(QString::fromStdString(item.id))
                                               .arg(QString::fromStdString(item.oldVersion)));
            if (!fuzzyRef) {
                this->printer.printErr(fuzzyRef.error());
                return -1;
            }
            fuzzyRefs.emplace_back(std::move(*fuzzyRef));
        }
    }

    api::types::v1::PackageManager1UpdateParameters params;
    for (const auto &fuzzyRef : fuzzyRefs) {
        api::types::v1::PackageManager1Package package;
        package.id = fuzzyRef.id.toStdString();
        if (fuzzyRef.channel) {
            package.channel = fuzzyRef.channel->toStdString();
        }
        if (fuzzyRef.version) {
            package.version = fuzzyRef.version->toString().toStdString();
        }
        params.packages.emplace_back(std::move(package));
    }

    auto pendingReply = this->pkgMan.Update(utils::serialize::toQVariantMap(params));
    pendingReply.waitForFinished();

    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(
        pendingReply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (result->code != 0) {
        auto err = LINGLONG_ERRV(QString::fromStdString(result->message), result->code);
        this->printer.printErr(err);
        return -1;
    }

    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    auto conn = pkgMan.connection();
    task = new api::dbus::v1::Task1(pkgMan.service(), taskObjectPath, conn);

    if (!conn.connect(pkgMan.service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qCritical() << "connect PropertiesChanged failed:" << conn.lastError();
        Q_ASSERT(false);
        return -1;
    }

    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        qCritical() << "connect taskDone failed";
        return -1;
    }
    loop.exec();

    if (this->lastState != linglong::api::types::v1::State::Succeed) {
        return -1;
    }

    return 0;
}

int Cli::search()
{
    LINGLONG_TRACE("command search");

    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = options.appid,
    };

    auto pendingReply = this->pkgMan.Search(utils::serialize::toQVariantMap(params));
    pendingReply.waitForFinished();

    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1JobInfo>(
      pendingReply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (!result->id) {
        this->printer.printErr(
          LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
        return -1;
    }

    QEventLoop loop;

    connect(&this->pkgMan,
            &api::dbus::v1::PackageManager::SearchFinished,
            [&](const QString &jobID, const QVariantMap &data) {
                // Note: once an error occurs, remember to return after exiting the loop.
                if (result->id->c_str() != jobID) {
                    return;
                }
                auto result =
                  utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(
                    data);
                if (!result) {
                    this->printer.printErr(result.error());
                    loop.exit(-1);
                    return;
                }
                // Note: should check return code of PackageManager1SearchResult
                if (result->code != 0) {
                    this->printer.printErr(
                      LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
                    loop.exit(result->code);
                    return;
                }

                if (!result->packages.has_value()) {
                    this->printer.printPackages({});
                    loop.exit(0);
                    return;
                }
                std::vector<api::types::v1::PackageInfoV2> pkgs;

                if (options.showDevel) {
                    pkgs = *result->packages;
                } else {
                    for (const auto &info : result->packages.value()) {
                        if (info.packageInfoV2Module == "develop") {
                            continue;
                        }

                        pkgs.push_back(info);
                    }
                }

                if (!options.type.empty()) {
                    filterPackageInfosFromType(pkgs, options.type);
                }

                this->printer.printPackages(pkgs);
                loop.exit(0);
            });
    return loop.exec();
}

int Cli::prune()
{
    LINGLONG_TRACE("command prune");

    QDBusInterface dbusIntrospect(this->pkgMan.service(),
                                  this->pkgMan.path(),
                                  "org.freedesktop.DBus.Introspectable",
                                  this->pkgMan.connection());
    QDBusReply<QString> authReply = dbusIntrospect.call("Introspect");
    if (!authReply.isValid() && authReply.error().type() == QDBusError::AccessDenied) {
        auto ret = this->runningAsRoot();
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        return -1;
    }

    QEventLoop loop;
    QString jobIDReply = "";
    auto ret = connect(
      &this->pkgMan,
      &api::dbus::v1::PackageManager::PruneFinished,
      [this, &loop, &jobIDReply](const QString &jobID, const QVariantMap &data) {
          if (jobIDReply != jobID) {
              return;
          }
          auto ret =
            utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(data);
          if (!ret) {
              this->printer.printErr(ret.error());
              loop.exit(-1);
          }

          this->printer.printPruneResult(*ret->packages);
          loop.exit(0);
      });

    auto pendingReply = this->pkgMan.Prune();
    pendingReply.waitForFinished();

    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result = utils::serialize::fromQVariantMap<api::types::v1::PackageManager1JobInfo>(
      pendingReply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (!result->id) {
        this->printer.printErr(
          LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
        return -1;
    }

    jobIDReply = QString::fromStdString(result->id.value());

    return loop.exec();
}

int Cli::uninstall()
{
    LINGLONG_TRACE("command uninstall");

    QDBusInterface dbusIntrospect(this->pkgMan.service(),
                                  this->pkgMan.path(),
                                  "org.freedesktop.DBus.Introspectable",
                                  this->pkgMan.connection());
    QDBusReply<QString> authReply = dbusIntrospect.call("Introspect");
    if (!authReply.isValid() && authReply.error().type() == QDBusError::AccessDenied) {
        auto ret = this->runningAsRoot();
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(options.appid));
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               {
                                                 .forceRemote = false,
                                                 .fallbackToRemote = false,
                                               });
    if (!ref) {
        this->printer.printErr(ref.error());
        return -1;
    }

    auto layerDir = this->repository.getLayerDir(*ref);
    if (!layerDir) {
        this->printer.printErr(layerDir.error());
        return -1;
    }

    auto info = layerDir->info();
    if (!info) {
        this->printer.printErr(info.error());
        return -1;
    }

    if (info->kind != "app") {
        this->printer.printErr(
          LINGLONG_ERRV("This layer is not an application, please use 'll-cli prune'.", -1));
        return -1;
    }

    auto params = api::types::v1::PackageManager1UninstallParameters{};
    params.package.id = fuzzyRef->id.toStdString();
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel->toStdString();
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version->toString().toStdString();
    }

    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
    }
    auto pendingReply = this->pkgMan.Uninstall(utils::serialize::toQVariantMap(params));
    pendingReply.waitForFinished();

    if (pendingReply.isError()) {
        if (pendingReply.error().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(
        pendingReply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (result->code != 0) {
        auto err = LINGLONG_ERRV(QString::fromStdString(result->message), result->code);
        if (result->type == "notification") {
            this->notifier->notify(
              api::types::v1::InteractionRequest{ .appName = "ll-cli",
                                                  .summary = result->message });
        } else {
            this->printer.printErr(err);
        }

        return -1;
    }

    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    auto conn = pkgMan.connection();
    task = new api::dbus::v1::Task1(pkgMan.service(), taskObjectPath, conn);

    if (!conn.connect(pkgMan.service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        qCritical() << "connect PropertiesChanged failed:" << conn.lastError();
        Q_ASSERT(false);
        return -1;
    }

    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        qCritical() << "connect taskDone failed";
        return -1;
    }
    loop.exec();

    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::list()
{
    LINGLONG_TRACE("command list");

    if (!options.showUpgradeList) {
        auto pkgs = this->repository.listLocal();
        if (!pkgs) {
            this->printer.printErr(pkgs.error());
            return -1;
        }

        if (!options.type.empty()) {
            filterPackageInfosFromType(*pkgs, options.type);
        }
        this->printer.printPackages(*pkgs);
        return 0;
    }

    auto upgradeList = this->listUpgradable(options.type);
    if (!upgradeList) {
        this->printer.printErr(upgradeList.error());
        return -1;
    }

    this->printer.printUpgradeList(*upgradeList);
    return 0;
}

utils::error::Result<std::vector<api::types::v1::UpgradeListResult>>
Cli::listUpgradable(const std::string &type)
{
    LINGLONG_TRACE("list upgradable");
    auto pkgs = this->repository.listLocalLatest();
    if (!pkgs) {
        return LINGLONG_ERR(pkgs);
    }

    if (!type.empty()) {
        filterPackageInfosFromType(*pkgs, options.type);
    }

    std::vector<api::types::v1::UpgradeListResult> upgradeList;
    auto fullFuzzyRef = package::FuzzyReference::parse(QString::fromStdString("."));
    if (!fullFuzzyRef) {
        return LINGLONG_ERR(fullFuzzyRef);
    }

    auto remotePkgs = this->repository.listRemote(*fullFuzzyRef);
    if (!remotePkgs) {
        return LINGLONG_ERR(remotePkgs);
    }

    utils::error::Result<package::Reference> reference = LINGLONG_ERR("reference not exists");
    for (const auto &pkg : *pkgs) {
        auto fuzzy = package::FuzzyReference::parse(QString::fromStdString(pkg.id));
        if (!fuzzy) {
            this->printer.printErr(fuzzy.error());
            continue;
        }

        reference = LINGLONG_ERR("reference not exists");

        for (auto record : *remotePkgs) {
            auto recordStr = nlohmann::json(record).dump();
            if (fuzzy->channel && fuzzy->channel->toStdString() != record.channel) {
                continue;
            }
            if (fuzzy->id.toStdString() != record.id) {
                continue;
            }
            auto version = package::Version::parse(QString::fromStdString(record.version));
            if (!version) {
                qWarning() << "Ignore invalid package record" << recordStr.c_str()
                           << version.error();
                continue;
            }
            if (record.arch.empty()) {
                qWarning() << "Ignore invalid package record";
                continue;
            }

            auto arch = package::Architecture::parse(record.arch[0]);
            if (!arch) {
                qWarning() << "Ignore invalid package record" << recordStr.c_str() << arch.error();
                continue;
            }
            auto channel = QString::fromStdString(record.channel);
            auto currentRef = package::Reference::create(channel, fuzzy->id, *version, *arch);
            if (!currentRef) {
                qWarning() << "Ignore invalid package record" << recordStr.c_str()
                           << currentRef.error();
                continue;
            }
            if (!reference) {
                reference = *currentRef;
                continue;
            }

            if (reference->version >= currentRef->version) {
                continue;
            }

            reference = *currentRef;
        }

        if (!reference) {
            std::cerr << "Failed to find the package: " << fuzzy->id.toStdString()
                      << ", maybe it is local package, skip it." << std::endl;
            continue;
        }

        auto oldVersion = package::Version::parse(QString::fromStdString(pkg.version));
        if (!oldVersion) {
            std::cerr << "failed to parse old version:"
                      << oldVersion.error().message().toStdString() << std::endl;
        }

        if (reference->version <= *oldVersion) {
            qDebug() << "The local package" << QString::fromStdString(pkg.id)
                     << "version is higher than the remote repository version, skip it.";
            continue;
        }

        upgradeList.emplace_back(api::types::v1::UpgradeListResult{
          .id = pkg.id,
          .newVersion = reference->version.toString().toStdString(),
          .oldVersion = oldVersion->toString().toStdString() });
    }
    return upgradeList;
}

int Cli::repo(CLI::App *app)
{
    LINGLONG_TRACE("command repo");

    auto propCfg = this->pkgMan.configuration();
    // check error here, this operation could be failed
    if (this->pkgMan.lastError().isValid()) {
        if (this->pkgMan.lastError().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(this->pkgMan.lastError().message());
        this->printer.printErr(err);
        return -1;
    }

    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfig>(propCfg);
    if (!cfg) {
        qCritical() << cfg.error();
        qCritical() << "linglong bug detected.";
        std::abort();
    }

    auto argsParseFunc = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParseFunc("show")) {
        this->printer.printRepoConfig(*cfg);
        return 0;
    }

    if (argsParseFunc("modify")) {
        this->printer.printErr(
          LINGLONG_ERRV("sub-command 'modify' already has been deprecated, please use sub-command "
                        "'add' to add a remote repository and use it as default."));
        return EINVAL;
    }

    std::string url = options.repoUrl;

    if (argsParseFunc("add") || argsParseFunc("update")) {
        if (url.rfind("http", 0) != 0) {
            this->printer.printErr(LINGLONG_ERRV(QString{ "url is invalid: " } + url.c_str()));
            return EINVAL;
        }

        // remove last slash
        if (options.repoUrl.back() == '/') {
            options.repoUrl.pop_back();
        }
    }

    std::string name = options.repoName;
    auto &cfgRef = *cfg;

    if (argsParseFunc("add")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return EINVAL;
        }

        auto ret = cfgRef.repos.try_emplace(name, url);
        if (!ret.second) {
            this->printer.printErr(
              LINGLONG_ERRV(QString{ "repo " } + name.c_str() + " already exist."));
            return -1;
        }
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    auto existingRepo = cfgRef.repos.find(name);
    if (existingRepo == cfgRef.repos.cend()) {
        this->printer.printErr(
          LINGLONG_ERRV(QString{ "the operated repo " } + name.c_str() + " doesn't exist"));
        return -1;
    }

    if (argsParseFunc("remove")) {
        if (cfgRef.defaultRepo == name) {
            this->printer.printErr(
              LINGLONG_ERRV(QString{ "repo " } + name.c_str()
                            + "is default repo, please change default repo before removing it."));
            return -1;
        }

        cfgRef.repos.erase(existingRepo);
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParseFunc("update")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return -1;
        }

        existingRepo->second = url;
        return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
    }

    if (argsParseFunc("set-default")) {
        if (cfgRef.defaultRepo != name) {
            cfgRef.defaultRepo = name;
            return this->setRepoConfig(utils::serialize::toQVariantMap(cfgRef));
        }

        return 0;
    }

    this->printer.printErr(LINGLONG_ERRV("unknown operation"));
    return -1;
}

int Cli::setRepoConfig(const QVariantMap &config)
{
    LINGLONG_TRACE("set repo config");

    QDBusInterface dbusIntrospect(this->pkgMan.service(),
                                  this->pkgMan.path(),
                                  "org.freedesktop.DBus.Introspectable",
                                  this->pkgMan.connection());
    QDBusReply<QString> authReply = dbusIntrospect.call("Introspect");
    if (!authReply.isValid() && authReply.error().type() == QDBusError::AccessDenied) {
        auto ret = this->runningAsRoot();
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        return -1;
    }

    this->pkgMan.setConfiguration(config);
    if (this->pkgMan.lastError().isValid()) {
        if (this->pkgMan.lastError().type() == QDBusError::AccessDenied) {
            this->notifier->notify(api::types::v1::InteractionRequest{
              .summary = "Permission deny, please check whether you are running as root." });
            return -1;
        }

        auto err = LINGLONG_ERRV(this->pkgMan.lastError().message());
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

int Cli::info()
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
        auto fuzzyRef = package::FuzzyReference::parse(app);
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

int Cli::content()
{
    LINGLONG_TRACE("command content");

    QStringList contents{};

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(options.appid));
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

std::vector<std::string>
Cli::filePathMapping(const std::vector<std::string> &command) const noexcept
{
    std::vector<std::string> execArgs;
    // if the --file or --url option is specified, need to map the file path to the linglong
    // path(/run/host).
    auto replaceIfSymlink = [](std::filesystem::path &file) {
        std::error_code ec;
        if (std::filesystem::is_symlink(file, ec)) {
            file = std::filesystem::read_symlink(file, ec);
            if (ec) {
                qInfo() << "resolve symlink " << file.c_str() << " error: " << ec.message().c_str()
                        << ", passing the file path to app as it is.";
            }
        }
    };

    for (const auto &arg : command) {
        if (arg.substr(0, 2) != "%%") {
            execArgs.emplace_back(arg);
            continue;
        }

        if (arg == "%%f") {
            std::filesystem::path file = options.filePath;
            if (file.empty()) {
                continue;
            }

            replaceIfSymlink(file);
            execArgs.emplace_back(LINGLONG_HOST_PATH / file);
            continue;
        }

        if (arg == "%%u") {
            if (options.fileUrl.empty()) {
                continue;
            }

            // if url is "file://" format, need to map the file path to the linglong path, or
            // deliver url directly.
            constexpr std::string_view filePrefix = "file://";
            if (options.fileUrl.rfind(filePrefix, 0) == 0) {
                std::filesystem::path nativePath = options.fileUrl.substr(filePrefix.size());
                replaceIfSymlink(nativePath);
                execArgs.emplace_back(std::string{ filePrefix }
                                      + (LINGLONG_HOST_PATH / nativePath).string());
            } else {
                execArgs.emplace_back(options.fileUrl);
            }

            continue;
        }

        qWarning() << "unkown command argument" << QString::fromStdString(arg);
    }

    return execArgs;
}

void Cli::filterPackageInfosFromType(std::vector<api::types::v1::PackageInfoV2> &list,
                                     const std::string &type) noexcept
{
    // if type is all, do nothing, return app of all packages.
    if (type == "all") {
        return;
    }

    std::vector<api::types::v1::PackageInfoV2> temp;

    // if type is runtime or app, return app of specific type.
    for (const auto &info : list) {
        if (info.kind == type) {
            temp.push_back(info);
        }
    }

    list.clear();
    std::move(temp.begin(), temp.end(), std::back_inserter(list));
}

utils::error::Result<void> Cli::runningAsRoot()
{
    LINGLONG_TRACE("run with pkexec");

    const char *pkexecBin = "pkexec";
    QStringList argv{ pkexecBin, "--keep-cwd" };
    argv.append(QCoreApplication::instance()->arguments());
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

} // namespace linglong::cli
