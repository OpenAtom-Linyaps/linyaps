/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1ResultWithTaskID.hpp"
#include "linglong/package/layer_file.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <nlohmann/json.hpp>

#include <iostream>

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
    ll-cli [--json] [--no-dbus] install TIER
    ll-cli [--json] uninstall TIER [--all] [--prune]
    ll-cli [--json] upgrade TIER
    ll-cli [--json] search [--type=TYPE] TEXT
    ll-cli [--json] [--no-dbus] list [--type=TYPE]
    ll-cli [--json] repo modify [--name=REPO] URL
    ll-cli [--json] repo list
    ll-cli [--json] info LAYER

Arguments:
    APP     Specify the application.
    PAGODA  Specify the pagodas (container).
    TIER    Specify the tier (container layer).
    URL     Specify the new repo URL.
    TEXT    The text used to search tiers.
    LAYER   Specify the layer path

Options:
    -h --help                 Show this screen.
    --version                 Show version.
    --json                    Use json to output command result.
    --no-dbus                 Use peer to peer DBus, this is used only in case that DBus daemon is not available.
    --no-dbus-proxy           Do not enable linglong-dbus-proxy.
    --dbus-proxy-cfg=PATH     Path of config of linglong-dbus-proxy.
    --file=FILE               you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --url=URL                 you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --working-directory=PATH  Specify working directory.
    --type=TYPE               Filter result with tiers type. One of "lib", "app" or "dev". [default: app]
    --state=STATE             Filter result with the tiers install state. Should be "local" or "remote". [default: local]
    --prune                   Remove application data if the tier is an application and all version of that application has been removed.

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
    info       Display the information of layer
)";

void Cli::processDownloadStatus(const QString &recTaskID,
                                const QString &percentage,
                                const QString &message,
                                int status)
{
    LINGLONG_TRACE("download status")

    if (recTaskID != this->taskID) {
        return;
    }

    this->lastStatus = static_cast<service::InstallTask::Status>(status);
    switch (status) {
    case service::InstallTask::Canceled:
    case service::InstallTask::Queued:
    case service::InstallTask::preInstall:
    case service::InstallTask::installBase:
    case service::InstallTask::installRuntime:
    case service::InstallTask::installApplication:
        [[fallthrough]];
    case service::InstallTask::postInstall: {
        this->printer.printTaskStatus(percentage, message, status);
    } break;
    case service::InstallTask::Success: {
        this->taskDone = true;
        this->printer.printTaskStatus(percentage, message, status);
        std::cout << std::endl;
    } break;
    case service::InstallTask::Failed: {
        this->printer.printErr(LINGLONG_ERRV("\n" + message));
        this->taskDone = true;
    }
    }
}

Cli::Cli(Printer &printer,
         ocppi::cli::CLI &ociCLI,
         runtime::ContainerBuilder &containerBuilder,
         api::dbus::v1::PackageManager &pkgMan,
         repo::OSTreeRepo &repo,
         QObject *parent)
    : QObject(parent)
    , printer(printer)
    , ociCLI(ociCLI)
    , containerBuilder(containerBuilder)
    , repository(repo)
    , pkgMan(pkgMan)
{
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

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               {
                                                 .forceRemote = false,
                                                 .fallbackToRemote = false,
                                               });
    if (!ref) {
        this->printer.printErr(ref.error());
        return -1;
    }

    auto layerDir = this->repository.getLayerDir(*ref, false);
    if (!layerDir) {
        this->printer.printErr(layerDir.error());
        return -1;
    }

    auto info = layerDir->info();
    if (!info) {
        this->printer.printErr(info.error());
        return -1;
    }

    std::optional<package::LayerDir> runtimeLayerDir;

    if (info->runtime) {
        auto runtimeFuzzyRef =
          package::FuzzyReference::parse(QString::fromStdString(*info->runtime));
        if (!runtimeFuzzyRef) {
            this->printer.printErr(runtimeFuzzyRef.error());
            return -1;
        }

        auto runtimeRef = this->repository.clearReference(*runtimeFuzzyRef,
                                                          {
                                                            .forceRemote = false,
                                                            .fallbackToRemote = false,
                                                          });
        if (!runtimeRef) {
            this->printer.printErr(runtimeRef.error());
            return -1;
        }

        auto layerDir = this->repository.getLayerDir(*runtimeRef);
        if (!layerDir) {
            this->printer.printErr(layerDir.error());
            return -1;
        }

        runtimeLayerDir = *layerDir;
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

    auto baseLayerDir = this->repository.getLayerDir(*baseRef);
    if (!baseLayerDir) {
        this->printer.printErr(LINGLONG_ERRV(baseLayerDir));
        return -1;
    }

    std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
    if (info->permissions && info->permissions->binds) {
        const auto &binds = info->permissions->binds;
        std::for_each(binds->cbegin(),
                      binds->cend(),
                      [&applicationMounts](
                        const api::types::v1::ApplicationConfigurationPermissionsBind &bind) {
                          applicationMounts.push_back(
			    ocppi::runtime::config::types::Mount {
                            .destination = bind.destination,
                            .options = { { "rbind" } },
                            .source = bind.source,
                            .type = "bind",
                          });
                      });
    }

    auto container = this->containerBuilder.create({
      .appID = ref->id,
      .containerID = (ref->toString() + "-" + QUuid::createUuid().toString()).toUtf8().toBase64(),
      .runtimeDir = runtimeLayerDir,
      .baseDir = *baseLayerDir,
      .appDir = *layerDir,
      .patches = {},
      .mounts = std::move(applicationMounts),
    });
    if (!container) {
        this->printer.printErr(container.error());
        return -1;
    }

    ocppi::runtime::config::types::Process p;

    auto command = args["COMMAND"].asStringList();
    if (command.empty()) {
        command = info->command.value_or(std::vector<std::string>{});
    }

    if (command.empty()) {
        qWarning() << "invalid command found in package" << QString::fromStdString(info->appid);
        command = { "bash" };
    }

    p.args = std::vector<std::string>{};
    filePathMapping(args, command, *p.args);

    QStringList envList = utils::command::getUserEnv(utils::command::envList);
    if (!envList.isEmpty()) {
        p.env = p.env.value_or(std::vector<std::string>{});
    }
    for (const auto &env : envList) {
        p.env->push_back(env.toStdString());
    }

    auto result = (*container)->run(p);
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

    std::vector<std::string> command;
    if (args["COMMAND"].isStringList()) {
        command = args["COMMAND"].asStringList();
    } else {
        command = { std::string("bash") };
    }
    auto result = this->ociCLI.exec(pagoda,
                                    command[0],
                                    std::vector<std::string>(command.begin() + 1, command.end()));
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }

    return 0;
}

int Cli::ps(std::map<std::string, docopt::value> & /*args*/)
{
    LINGLONG_TRACE("command ps");

    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    std::vector<api::types::v1::CliContainer> myContainers;
    for (const auto &container : *containers) {
        auto decodedID = QString(QByteArray::fromBase64(container.id.c_str()));
        auto pkgName = decodedID.left(decodedID.indexOf('-'));
        myContainers.push_back({
          .id = container.id,
          .package = pkgName.toStdString(),
          .pid = container.pid,
        });
    }

    this->printer.printContainers(myContainers);

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
    if (!this->taskDone) {
        this->pkgMan.CancelTask(this->taskID);
        std::cout << "cancel downloading application." << std::endl;
    }
}

int Cli::install(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command install");

    auto tier = args["TIER"].asString();
    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(tier));

    if (!fuzzyRef) {
        const auto layerFile = package::LayerFile::New(QString::fromStdString(tier));
        if (!layerFile) {
            qCritical() << layerFile.error();
            return -1;
        }
        if (!(*layerFile)->isReadable()) {
            qCritical() << QFileInfo(**layerFile).absoluteFilePath() << "is not readable.";
            return -1;
        }
        qInfo() << "install layer file" << QString::fromStdString(tier);
        QDBusUnixFileDescriptor dbusFileDescriptor((*layerFile)->handle());
        auto pendingReply = this->pkgMan.InstallLayer(dbusFileDescriptor);
        auto reply = pendingReply.value();
        auto result =
          utils::serialize::fromQVariantMap<api::types::v1::PackageManager1ResultWithTaskID>(reply);
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
        return 0;
    }

    auto conn = this->pkgMan.connection();
    auto con = conn.connect(
      this->pkgMan.service(),
      this->pkgMan.path(),
      this->pkgMan.interface(),
      "TaskChanged",
      this,
      SLOT(processDownloadStatus(const QString &, const QString &, const QString &, int)));
    if (!con) {
        qCritical() << "Failed to connect signal: TaskChanged. state may be incorrect.";
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

    auto pendingReply = this->pkgMan.Install(utils::serialize::toQVariantMap(params));
    auto reply = pendingReply.value();
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1ResultWithTaskID>(reply);
    if (!result) {
        qCritical() << "bug detected.";
        std::abort();
    }

    if (result->code != 0) {
        this->printer.printReply({ .code = result->code, .message = result->message });
        return -1;
    }

    this->taskID = QString::fromStdString(*result->taskID);
    this->taskDone = false;
    QEventLoop loop;
    std::function<void()> statusChecker = std::function{ [&loop, &statusChecker, this]() -> void {
        if (this->taskDone) {
            loop.exit(0);
        }
        QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
    } };

    QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
    loop.exec();

    // Call ReloadApplications() in AM for now. Remove later.
    if ((QSysInfo::productType() == "uos" || QSysInfo::productType() == "Deepin")
        && this->lastStatus == service::InstallTask::Success) {
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
    return 0;
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

    auto conn = this->pkgMan.connection();
    auto con = conn.connect(
      this->pkgMan.service(),
      this->pkgMan.path(),
      this->pkgMan.interface(),
      "TaskChanged",
      this,
      SLOT(processDownloadStatus(const QString &, const QString &, const QString &, int)));
    if (!con) {
        qCritical() << "Failed to connect signal: TaskChanged. state may be incorrect.";
        return -1;
    }

    auto reply = this->pkgMan.Update(utils::serialize::toQVariantMap(params)).value();
    auto result =
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1ResultWithTaskID>(reply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (result->code != 0) {
        auto err = LINGLONG_ERRV(QString::fromStdString(result->message), result->code);
        this->printer.printErr(err);
        return -1;
    }

    this->taskID = QString::fromStdString(*result->taskID);
    this->taskDone = false;
    QEventLoop loop;
    std::function<void()> statusChecker = std::function{ [&loop, &statusChecker, this]() -> void {
        if (this->taskDone) {
            loop.exit(0);
        }
        QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
    } };

    QMetaObject::invokeMethod(&loop, statusChecker, Qt::QueuedConnection);
    loop.exec();

    if (this->lastStatus != service::InstallTask::Success) {
        return -1;
    }

    // Call ReloadApplications() in AM for now. Remove later.
    if ((QSysInfo::productType() == "uos" || QSysInfo::productType() == "Deepin")
        && this->lastStatus == service::InstallTask::Success) {
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

    return this->lastStatus == service::InstallTask::Success ? 0 : -1;
}

int Cli::search(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command search");

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
      utils::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(reply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    if (!result->packages) {
        this->printer.printErr(
          LINGLONG_ERRV("\n" + QString::fromStdString(result->message), result->code));
        return -1;
    }

    this->printer.printPackages(*result->packages);
    return 0;
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

    auto params = api::types::v1::PackageManager1UninstallParameters{};
    params.package.id = fuzzyRef->id.toStdString();
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel->toStdString();
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version->toString().toStdString();
    }

    auto reply = this->pkgMan.Uninstall(utils::serialize::toQVariantMap(params));
    reply.waitForFinished();
    if (!reply.isValid()) {
        this->printer.printErr(LINGLONG_ERRV(reply.error().message(), reply.error().type()));
        return -1;
    }
    auto result = utils::serialize::fromQVariantMap<api::types::v1::CommonResult>(reply.value());
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }
    if (result->code != 0) {
        this->printer.printErr(
          LINGLONG_ERRV(QString::fromStdString(result->message), result->code));
        return -1;
    }

    return 0;
}

int Cli::list(std::map<std::string, docopt::value> & /*args*/)
{
    auto pkgs = this->repository.listLocal();
    if (!pkgs) {
        this->printer.printErr(pkgs.error());
        return -1;
    }

    this->printer.printPackages(*pkgs);
    return 0;
}

int Cli::repo(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command repo");

    auto propCfg = this->pkgMan.configuration();
    auto tmp = propCfg.value("repos");

    auto cfg = utils::serialize::fromQVariantMap<api::types::v1::RepoConfig>(propCfg);
    if (!cfg) {
        qCritical() << cfg.error();
        qCritical() << "linglong bug detected.";
        std::abort();
    }

    if (!args["modify"].asBool()) {
        this->printer.printRepoConfig(*cfg);
        return 0;
    }

    QString name;
    QString url;
    if (args["--name"].isString()) {
        name = QString::fromStdString(args["--name"].asString());
    }
    if (args["URL"].isString()) {
        url = QString::fromStdString(args["URL"].asString());
    }

    cfg->repos[name.toStdString()] = url.toStdString();
    cfg->defaultRepo = name.toStdString();
    this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(*cfg));
    return 0;
}

int Cli::info(std::map<std::string, docopt::value> &args)
{
    LINGLONG_TRACE("command info");

    QString layerPath;
    if (args["LAYER"].isString()) {
        layerPath = QString::fromStdString(args["LAYER"].asString());
    }

    if (layerPath.isEmpty()) {
        auto err = LINGLONG_ERR("failed to get layer path").value();
        this->printer.printErr(err);
        return err.code();
    }

    const auto layerFile = package::LayerFile::New(layerPath);

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

void Cli::filePathMapping(std::map<std::string, docopt::value> &args,
                          const std::vector<std::string> &command,
                          std::vector<std::string> &execArgs) const noexcept
{
    std::string targetHostPath;

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

            const QString filePre = "file:///";

            // if url is "file:///" format, need to map the file path to the linglong path, or
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
}

} // namespace linglong::cli
