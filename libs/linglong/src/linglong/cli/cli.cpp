/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "linglong/api/types/v1/CommonResult.hpp"
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

using namespace linglong::utils::error;

namespace linglong::cli {

const char Cli::USAGE[] =
  R"(linglong CLI
A CLI program to run application and manage linglong pagoda and tiers.

Usage:
    ll-cli [--json] --version
    ll-cli [--json] run APP [--no-dbus-proxy] [--dbus-proxy-cfg=PATH] ( [--file=FILE] | [--url=URL] ) [--] [COMMAND...]
    ll-cli [--json] ps
    ll-cli [--json] exec PAGODA [--working-directory=PATH] [--] COMMAND...
    ll-cli [--json] enter PAGODA [--working-directory=PATH] [--] [COMMAND...]
    ll-cli [--json] kill PAGODA
    ll-cli [--json] [--no-dbus] install TIER [--force]
    ll-cli [--json] uninstall TIER [--all] [--prune]
    ll-cli [--json] upgrade TIER [--force]
    ll-cli [--json] search [--type=TYPE] [--dev] TEXT
    ll-cli [--json] [--no-dbus] list [--type=TYPE]
    ll-cli [--json] repo modify [--name=REPO] URL
    ll-cli [--json] repo add NAME URL
    ll-cli [--json] repo remove NAME
    ll-cli [--json] repo update NAME URL
    ll-cli [--json] repo set-default NAME
    ll-cli [--json] repo show
    ll-cli [--json] info TIER
    ll-cli [--json] content APP
    ll-cli [--json] migrate
    ll-cli [--json] prune

Arguments:
    APP     Specify the application.
    PAGODA  Specify the pagodas (container).
    TIER    Specify the tier (container layer).
    NAME    Specify the repo name.
    URL     Specify the repo URL.
    TEXT    The text used to search tiers.

Options:
    -h --help                 Show help information of ll-cli.
    --version                 Show version.
    --json                    Use json to output command result, you can get the description of the output refer to linglong api documentation(/usr/share/linglong/api/api.json).
    --no-dbus                 Use peer to peer DBus, this is used only in case that DBus daemon is not available.
    --no-dbus-proxy           Do not enable linglong-dbus-proxy.
    --dbus-proxy-cfg=PATH     Path of config of linglong-dbus-proxy.
    --file=FILE               you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --url=URL                 you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --working-directory=PATH  Specify working directory.
    --type=TYPE               Filter result with tiers type. One of "runtime", "app" or "all". [default: app]
    --state=STATE             Filter result with the tiers install state. Should be "local" or "remote". [default: local]
    --prune                   Remove application data if the tier is an application and all version of that application has been removed.
    --dev                     include develop tiers in result.

Subcommands:
    run        Run an application.
    ps         List all pagodas.
    exec       Execute command in a pagoda.
    enter      Enter a pagoda.
    kill       Stop applications and remove the pagoda.
    install    Install tier(s).
    uninstall  Uninstall tier(s).
    upgrade    Upgrade tier(s).
    search     Search for tiers.
    list       List known tiers.
    repo       Display or modify information of the repository currently using.
    info       Display the information of layer.
    content    Display the exported files of application.
    prune      Remove the unused base or runtime.
)";

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

void Cli::onTaskAdded([[maybe_unused]] QDBusObjectPath object_path)
{
    qDebug() << "task added" << object_path.path();
}

void Cli::onTaskRemoved(QDBusObjectPath object_path, int state, int subState, QString message)
{
    if (object_path.path() != taskObjectPath) {
        return;
    }

    delete task;
    task = nullptr;

    if (lastSubState != api::types::v1::SubState::Done) {
        lastState = static_cast<api::types::v1::State>(state);
        lastSubState = static_cast<api::types::v1::SubState>(subState);
        lastMessage = std::move(message);
        lastPercentage = 100;
        printProgress();
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
                      SLOT(onTaskRemoved(QDBusObjectPath, int, int, QString)))) {
        qFatal("couldn't connect to package manager signal 'TaskRemoved'");
    }
}

int Cli::run(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command run");

    const auto userInputAPP = QString::fromStdString(args["APP"].asString());
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

    auto appLayerDir = this->repository.getLayerDir(*curAppRef, std::string{ "binary" });
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

        auto runtimeLayerDirRet =
          this->repository.getLayerDir(*runtimeRefRet, std::string{ "binary" }, info->uuid);
        if (!runtimeLayerDirRet) {
            this->printer.printErr(runtimeLayerDirRet.error());
            return -1;
        }

        runtimeLayerRef = runtimeRefRet->toString().toStdString();
        runtimeLayerDir = std::move(runtimeLayerDirRet).value();
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

    auto baseLayerDir = this->repository.getLayerDir(*baseRef, std::string{ "binary" }, info->uuid);
    if (!baseLayerDir) {
        this->printer.printErr(LINGLONG_ERRV(baseLayerDir));
        return -1;
    }

    auto command = args["COMMAND"].asStringList();
    if (command.empty()) {
        command = info->command.value_or(std::vector<std::string>{});
    }
    if (command.empty()) {
        qWarning() << "invalid command found in package" << QString::fromStdString(info->id);
        command = { "bash" };
    }
    auto execArgs = filePathMapping(args, command);

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
                              runtime = runtimeLayerRef.value(),
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

int Cli::exec(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("ll-cli exec");

    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    auto pagoda = args["PAGODA"].asString();
    for (const auto &container : *containers) {
        auto decodedID = QString(QByteArray::fromBase64(container.id.c_str()));
        if (!decodedID.startsWith(QString::fromStdString(pagoda))) {
            continue;
        }
        pagoda = container.id;
        break;
    }

    qInfo() << "select pagoda" << QString::fromStdString(pagoda);

    std::vector<std::string> command = args["COMMAND"].asStringList();
    if (command.size() != 0) {
        QStringList bashArgs;
        // 为避免原始args包含空格，每个arg都使用单引号包裹，并对arg内部的单引号进行转义替换
        for (const auto &arg : command) {
            bashArgs.push_back(
              QString("'%1'").arg(QString::fromStdString(arg).replace("'", "'\\''")));
        }

        if (!bashArgs.isEmpty()) {
            // exec命令使用原始args中的进程替换bash进程
            bashArgs.prepend("exec");
        }
        // 在原始args前面添加bash --login -c，这样可以使用/etc/profile配置的环境变量
        command = std::vector<std::string>{ "/bin/bash",
                                            "--login",
                                            "-c",
                                            bashArgs.join(" ").toStdString(),
                                            "; wait" };
    } else {
        command = { "bash", "--login" };
    }

    auto opt = ocppi::runtime::ExecOption{};
    opt.uid = ::getuid();
    opt.gid = ::getgid();

    auto result =
      this->ociCLI.exec(pagoda, command[0], { command.begin() + 1, command.end() }, opt);
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

int Cli::ps(std::map<std::string, docopt::value> & /*args*/)
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

int Cli::kill(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command kill");

    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    auto pagoda = args["PAGODA"].asString();
    for (const auto &container : *containers) {
        auto decodedID = QString(QByteArray::fromBase64(container.id.c_str()));
        if (!decodedID.startsWith(QString::fromStdString(pagoda))) {
            continue;
        }
        pagoda = container.id;
        break;
    }

    qInfo() << "select pagoda" << QString::fromStdString(pagoda);

    auto result =
      this->ociCLI.kill(ocppi::runtime::ContainerID(pagoda), ocppi::runtime::Signal("SIGTERM"));
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }

    return 0;
}

void Cli::cancelCurrentTask()
{
    if ((this->lastSubState != linglong::api::types::v1::SubState::Done) && this->task != nullptr) {
        this->task->Cancel();
        std::cout << "cancel running task." << std::endl;
    }
}

int Cli::installFromFile(const QFileInfo &fileInfo)
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
    QDBusUnixFileDescriptor dbusFileDescriptor(file.handle());

    auto pendingReply = this->pkgMan.InstallFromFile(dbusFileDescriptor, fileInfo.suffix());
    pendingReply.waitForFinished();
    if (!pendingReply.isFinished()) {
        auto err = LINGLONG_ERRV("dbus early return");
        this->printer.printErr(err);
        return -1;
    }
    if (pendingReply.isError()) {
        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto reply = pendingReply.value();
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(reply);
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

    updateAM();
    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

void Cli::updateAM() noexcept
{
    // Call ReloadApplications() in AM for now. Remove later.
    if ((QSysInfo::productType() == "uos" || QSysInfo::productType() == "Deepin")
        && this->lastState == linglong::api::types::v1::State::Succeed) {
        QDBusConnection conn = QDBusConnection::sessionBus();
        if (!conn.isConnected()) {
            qWarning() << "Failed to connect to the session bus";
        }
        QDBusMessage msg = QDBusMessage::createMethodCall("org.desktopspec.ApplicationManager1",
                                                          "/org/desktopspec/ApplicationManager1",
                                                          "org.desktopspec.ApplicationManager1",
                                                          "ReloadApplications");
        auto ret = QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
        if (ret.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "call reloadApplications failed:" << ret.errorMessage();
        }
    }
}

int Cli::install(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command install");

    auto tier = args["TIER"].asString();
    QFileInfo info(QString::fromStdString(tier));

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(info.absoluteFilePath());
    }

    api::types::v1::PackageManager1InstallParameters params;
    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(tier));
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

    auto pendingReply = this->pkgMan.Install(utils::serialize::toQVariantMap(params));
    auto reply = pendingReply.value();
    if (!pendingReply.isFinished()) {
        auto err = LINGLONG_ERRV("dbus early return");
        this->printer.printErr(err);
        return -1;
    }
    if (pendingReply.isError()) {
        auto err = LINGLONG_ERRV(pendingReply.error().message());
        this->printer.printErr(err);
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(reply);
    if (!result) {
        qCritical() << "bug detected:" << result.error().message();
        std::abort();
    }

    if (result->code != 0) {
        this->printer.printReply({ .code = result->code, .message = result->message });
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

    updateAM();
    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::upgrade(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command upgrade");

    auto tier = args["TIER"].asString();

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(tier));
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    api::types::v1::PackageManager1InstallParameters params;
    params.package.id = fuzzyRef->id.toStdString();
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel->toStdString();
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version->toString().toStdString();
    }

    auto reply = this->pkgMan.Update(utils::serialize::toQVariantMap(params)).value();
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(reply);
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

    updateAM();
    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::search(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command search");

    QString type;
    bool isShowDev = false;

    if (args["--type"].isString()) {
        type = QString::fromStdString(args["--type"].asString());
    }

    if (args["--dev"].isBool()) {
        isShowDev = args["--dev"].asBool();
    }

    auto text = args["TEXT"].asString();
    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = text,
    };

    auto reply = this->pkgMan.Search(utils::serialize::toQVariantMap(params));
    reply.waitForFinished();
    if (!reply.isValid()) {
        this->printer.printErr(LINGLONG_ERRV(reply.error().message(), reply.error().type()));
        return -1;
    }
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1JobInfo>(reply.value());
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
                if (result->id->c_str() != jobID) {
                    return;
                }
                auto result =
                  utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(
                    data);
                if (!result) {
                    this->printer.printErr(result.error());
                    loop.exit(-1);
                }
                std::vector<api::types::v1::PackageInfoV2> pkgs;

                if (isShowDev) {
                    pkgs = *result->packages;
                } else {
                    for (const auto &info : *result->packages) {
                        if (info.packageInfoV2Module == "develop") {
                            continue;
                        }

                        pkgs.push_back(info);
                    }
                }

                if (!type.isEmpty()) {
                    filterPackageInfosFromType(pkgs, type);
                }

                this->printer.printPackages(pkgs);
                loop.exit(0);
            });
    return loop.exec();
}

int Cli::prune(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command prune");

    auto reply = this->pkgMan.Prune();
    reply.waitForFinished();
    if (!reply.isValid()) {
        this->printer.printErr(LINGLONG_ERRV(reply.error().message(), reply.error().type()));
        return -1;
    }
    if (reply.isError()) {
        this->printer.printErr(LINGLONG_ERRV(reply.error().message(), -1));
        return -1;
    }

    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1JobInfo>(reply.value());
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
            &api::dbus::v1::PackageManager::PruneFinished,
            [&](const QString &jobID, const QVariantMap &data) {
                if (result->id->c_str() != jobID) {
                    return;
                }
                auto result =
                  utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(
                    data);
                if (!result) {
                    this->printer.printErr(result.error());
                    loop.exit(-1);
                }

                this->printer.printPruneResult(*result->packages);
                loop.exit(0);
            });
    return loop.exec();
}

int Cli::uninstall(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command uninstall");

    auto tier = args["TIER"].asString();

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(tier));
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

    auto reply = this->pkgMan.Uninstall(utils::serialize::toQVariantMap(params));
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1PackageTaskResult>(reply);
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

    return this->lastState == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::list(std::map<std::string, docopt::value> &args)
{
    QString type;

    if (args["--type"].isString()) {
        type = QString::fromStdString(args["--type"].asString());
    }

    auto pkgs = this->repository.listLocal();
    if (!pkgs) {
        this->printer.printErr(pkgs.error());
        return -1;
    }

    if (!type.isEmpty()) {
        filterPackageInfosFromType(*pkgs, type);
    }

    this->printer.printPackages(*pkgs);
    return 0;
}

int Cli::repo(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command repo");

    auto propCfg = this->pkgMan.configuration();
    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfig>(propCfg);
    if (!cfg) {
        qCritical() << cfg.error();
        qCritical() << "linglong bug detected.";
        std::abort();
    }
    auto &cfgRef = *cfg;

    if (args.empty() || args["show"].asBool()) {
        this->printer.printRepoConfig(*cfg);
        return 0;
    }

    if (args["modify"].asBool()) {
        this->printer.printErr(
          LINGLONG_ERRV("sub-command 'modify' already has been deprecated, please use sub-command "
                        "'add' to add a remote repository and use it as default."));
        return EINVAL;
    }

    std::string name;
    if (!args["NAME"].isString()) {
        this->printer.printErr(LINGLONG_ERRV("repo name must be specified as string"));
        return EINVAL;
    }
    name = args["NAME"].asString();

    std::string url;
    if (args["URL"].isString()) {
        // TODO: verify more complexly
        url = args["URL"].asString();
        if (url.rfind("http", 0) != 0) {
            this->printer.printErr(LINGLONG_ERRV(QString{ "url is invalid: " } + url.c_str()));
            return EINVAL;
        }

        // remove last slash
        if (url.back() == '/') {
            url.pop_back();
        }
    }

    if (args["add"].asBool()) {
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

        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
    }

    auto existingRepo = cfgRef.repos.find(name);
    if (existingRepo == cfgRef.repos.cend()) {
        this->printer.printErr(
          LINGLONG_ERRV(QString{ "the operated repo " } + name.c_str() + " doesn't exist"));
        return -1;
    }

    if (args["remove"].asBool()) {
        if (cfgRef.defaultRepo == name) {
            this->printer.printErr(
              LINGLONG_ERRV(QString{ "repo " } + name.c_str()
                            + "is default repo, please change default repo before removing it."));
            return -1;
        }

        cfgRef.repos.erase(existingRepo);
        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
    }

    if (args["update"].asBool()) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return -1;
        }

        existingRepo->second = url;
        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
    }

    if (args["set-default"].asBool()) {
        if (cfgRef.defaultRepo != name) {
            cfgRef.defaultRepo = name;
            this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        }

        return 0;
    }

    this->printer.printErr(LINGLONG_ERRV("unknown operation"));
    return -1;
}

int Cli::info(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command info");

    QString tier;
    if (args["TIER"].isString()) {
        tier = QString::fromStdString(args["TIER"].asString());
    }

    if (tier.isEmpty()) {
        auto err = LINGLONG_ERR("failed to get layer path").value();
        this->printer.printErr(err);
        return err.code();
    }

    QFileInfo file(tier);
    auto isLayerFile = file.isFile() && file.suffix() == "layer";

    // 如果是app，显示app tier信息
    if (!isLayerFile) {
        auto fuzzyRef = package::FuzzyReference::parse(tier);
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

        auto layer = this->repository.getLayerDir(*ref);
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

    // 如果是layer文件，显示layer文件 tier信息
    const auto layerFile = package::LayerFile::New(tier);

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

int Cli::content(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command content");

    QString tier;
    QStringList contents{};
    if (args["APP"].isString()) {
        tier = QString::fromStdString(args["APP"].asString());
    }

    auto fuzzyRef = package::FuzzyReference::parse(tier);
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

    auto layer = this->repository.getLayerDir(*ref);
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

int Cli::migrate([[maybe_unused]] std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("cli migrate")

    if (!notifier) {
        qCritical() << "there hasn't notifier, abort migrate.";
        return -1;
    }

    std::vector<std::string> actions{ "Yes", "No" };
    for (auto it = actions.begin(); it != actions.end(); ++it) {
        // May be we need localization in the future
        it = actions.insert(it + 1, *it);
    }

    api::types::v1::InteractionRequest req{
        .actions = actions,
        .body = "Do you want to migrate immediately?\nThis will stop all runnning applications.",
        .summary = "Package Manager needs to migrate data.",
    };

    while (true) {
        auto reply = notifier->request(req);
        if (!reply) {
            qCritical() << "internal error: notify failed";
            std::abort();
        }

        auto action = reply->action.value_or("No");
        if (action == "Yes") {
            qDebug() << "approve migration";
            break;
        }

        auto ret = notifier->notify(api::types::v1::InteractionRequest{
          .summary = "you could run 'll-cli migrate' on later to migrating data." });
        if (!ret) {
            this->printer.printErr(ret.error());
            return -1;
        }

        return 0;
    }

    // stop all running apps
    // FIXME: In multi-user conditions, we couldn't kill applications which started by different
    // user
    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    for (const auto &container : *containers) {
        auto result = this->ociCLI.kill(ocppi::runtime::ContainerID(container.id),
                                        ocppi::runtime::Signal("SIGTERM"));
        if (!result) {
            auto err = LINGLONG_ERRV(result);
            this->printer.printErr(err);
            return -1;
        }
    }

    // beginning migrating data
    if (!this->pkgMan.connection().connect(this->pkgMan.service(),
                                           "/org/deepin/linglong/Migrate1",
                                           "org.deepin.linglong.Migrate1",
                                           "MigrateDone",
                                           this,
                                           SLOT(forwardMigrateDone(int, QString)))) {
        qFatal("couldn't connect to dbus signal MigrateDone");
    }

    int retCode = std::numeric_limits<int>::min();
    QString retMsg;
    QObject::connect(this, &Cli::migrateDone, [&retCode, &retMsg](int newCode, QString newMsg) {
        retCode = newCode;
        retMsg = std::move(newMsg);
    });

    auto reply = this->pkgMan.Migrate();
    reply.waitForFinished();

    if (!reply.isValid()) {
        this->printer.printErr(LINGLONG_ERRV("invalid reply from migrate", -1));
        return -1;
    }

    if (reply.isError()) {
        this->printer.printErr(LINGLONG_ERRV(reply.error().message(), -1));
        return -1;
    }

    auto result = utils::serialize::fromQVariantMap<api::types::v1::CommonResult>(reply.value());
    if (!result) {
        auto err = LINGLONG_ERR(
          "internal bug detected, application will exit, but migration may already staring");
        this->printer.printErr(err.value());
        std::abort();
    }

    auto ret = notifier->notify(api::types::v1::InteractionRequest{ .summary = result->message });
    if (!ret) {
        auto err = LINGLONG_ERR(
          "internal bug detected, application will exit, but migration may already staring",
          ret.error());
        this->printer.printErr(err.value());
        std::abort();
    }

    QEventLoop loop;
    std::function<void()> statusChecker =
      std::function{ [&loop, &statusChecker, &retCode]() -> void {
          if (retCode != std::numeric_limits<int>::min()) {
              loop.exit(0);
          }
          QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
      } };
    QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
    loop.exec();

    ret =
      this->notifier->notify(api::types::v1::InteractionRequest{ .summary = retMsg.toStdString() });
    if (!ret) {
        this->printer.printReply({ .code = retCode, .message = retMsg.toStdString() });
        return -1;
    }

    return retCode;
}

std::vector<std::string>
Cli::filePathMapping(std::map<std::string, docopt::value> &args,
                     const std::vector<std::string> &command) const noexcept
{
    std::string targetHostPath;
    std::vector<std::string> execArgs;
    // if the --file or --url option is specified, need to map the file path to the linglong
    // path(/run/host).
    for (const auto &arg : command) {
        if (arg.substr(0, 2) != "%%") {
            execArgs.push_back(arg);
            continue;
        }

        if (arg == "%%f") {
            const auto file = args["--file"].asString();

            if (file.empty()) {
                continue;
            }

            targetHostPath = LINGLONG_HOST_PATH + file;
            execArgs.push_back(targetHostPath);
            continue;
        }

        if (arg == "%%u") {
            const auto url = QString::fromStdString(args["--url"].asString());

            if (url.isEmpty()) {
                continue;
            }

            const QString filePre = "file://";

            // if url is "file://" format, need to map the file path to the linglong path, or
            // deliver url directly.
            if (url.startsWith(filePre)) {
                const auto filePath = url.mid(filePre.length(), url.length() - filePre.length());
                targetHostPath =
                  filePre.toStdString() + LINGLONG_HOST_PATH + filePath.toStdString();
            } else {
                targetHostPath = url.toStdString();
            }

            execArgs.push_back(targetHostPath);
            continue;
        }

        qWarning() << "unkown command argument" << QString::fromStdString(arg);
    }

    return execArgs;
}

void Cli::filterPackageInfosFromType(std::vector<api::types::v1::PackageInfoV2> &list,
                                     const QString &type) noexcept
{
    // if type is all, do nothing, return tier of all packages.
    if (type == "all") {
        return;
    }

    std::vector<api::types::v1::PackageInfoV2> temp;

    // if type is runtime or app, return tier of specific type.
    for (const auto &info : list) {
        if (info.kind == type.toStdString()) {
            temp.push_back(info);
        }
    }

    list.clear();
    std::move(temp.begin(), temp.end(), std::back_inserter(list));
}

void Cli::forwardMigrateDone(int code, QString message)
{
    Q_EMIT migrateDone(code, message, {});
}

} // namespace linglong::cli
