/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "linglong/api/types/v1/CommonResult.hpp"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1ResultWithTaskID.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/package/layer_file.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/Signal.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <nlohmann/json.hpp>

#include <QEventLoop>
#include <QFileInfo>

#include <filesystem>
#include <iostream>

using namespace linglong::utils::error;

namespace linglong::cli {

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
}

int Cli::run()
{
    LINGLONG_TRACE("command run");

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
        if (info->uuid->empty()) {
            auto runtimeLayerDirRet = this->repository.getMergedModuleDir(*runtimeRef);
            if (!runtimeLayerDirRet) {
                this->printer.printErr(runtimeLayerDirRet.error());
                return -1;
            }
            runtimeLayerDir = *runtimeLayerDirRet;
        } else {
            auto runtimeLayerDirRet =
              this->repository.getLayerDir(*runtimeRef, std::string{ "binary" }, info->uuid);
            if (!runtimeLayerDirRet) {
                this->printer.printErr(runtimeLayerDirRet.error());
                return -1;
            }
            runtimeLayerDir = *runtimeLayerDirRet;
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
        qDebug() << "getLayerDir base" << info->uuid->c_str();
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

    auto containers = this->ociCLI.list().value_or(std::vector<ocppi::types::ContainerListItem>{});
    for (const auto &container : containers) {
        const auto &decodedID = QString(QByteArray::fromBase64(container.id.c_str()));
        if (!decodedID.startsWith(curAppRef->toString())) {
            continue;
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
      .containerID =
        (curAppRef->toString() + "-" + QUuid::createUuid().toString()).toUtf8().toBase64(),
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

    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    auto pagoda = options.pagoda;
    for (const auto &container : *containers) {
        auto decodedID = QString(QByteArray::fromBase64(container.id.c_str()));
        if (!decodedID.startsWith(QString::fromStdString(pagoda))) {
            continue;
        }
        pagoda = container.id;
        break;
    }

    qInfo() << "select pagoda" << QString::fromStdString(pagoda);

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
      this->ociCLI.exec(pagoda, commands[0], { commands.begin() + 1, commands.end() }, opt);
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }

    return 0;
}

int Cli::ps()
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

int Cli::kill()
{
    LINGLONG_TRACE("command kill");

    auto containers = this->ociCLI.list();
    if (!containers) {
        auto err = LINGLONG_ERRV(containers);
        this->printer.printErr(err);
        return -1;
    }

    auto pagoda = options.pagoda;
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

    updateAM();
    return 0;
}

void Cli::updateAM() noexcept
{
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
}

int Cli::install()
{
    LINGLONG_TRACE("command install");

    auto tier = QString::fromStdString(options.tier);
    QFileInfo info(tier);

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(info.absoluteFilePath());
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
    auto fuzzyRef = package::FuzzyReference::parse(tier);
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

    updateAM();
    return this->lastStatus == service::InstallTask::Success ? 0 : -1;
}

int Cli::upgrade()
{
    LINGLONG_TRACE("command upgrade");

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(options.tier));
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

    updateAM();
    return this->lastStatus == service::InstallTask::Success ? 0 : -1;
}

int Cli::search()
{
    LINGLONG_TRACE("command search");

    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = options.tier,
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

                if (options.showDevel) {
                    pkgs = *result->packages;
                } else {
                    for (const auto &info : *result->packages) {
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

int Cli::uninstall()
{
    LINGLONG_TRACE("command uninstall");

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(options.tier));
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

    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
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

int Cli::list()
{
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

int Cli::repo(CLI::App *app)
{
    LINGLONG_TRACE("command repo");

    auto propCfg = this->pkgMan.configuration();
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

        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
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
        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
    }

    if (argsParseFunc("update")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return -1;
        }

        existingRepo->second = url;
        this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        return 0;
    }

    if (argsParseFunc("set-default")) {
        if (cfgRef.defaultRepo != name) {
            cfgRef.defaultRepo = name;
            this->pkgMan.setConfiguration(utils::serialize::toQVariantMap(cfgRef));
        }

        return 0;
    }

    this->printer.printErr(LINGLONG_ERRV("unknown operation"));
    return -1;
}

int Cli::info()
{
    LINGLONG_TRACE("command info");

    QString tier = QString::fromStdString(options.tier);

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

int Cli::migrate()
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

    // connecting to this lambda before connecting the second one of the slot 'quit' of event loop
    // see comments below for details
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

    if (retCode == std::numeric_limits<int>::min()) {
        // If a signal is connected to several slots,
        // the slots are activated in the same order in which the connections were made,
        // when the signal is emitted.
        // refer: https://doc.qt.io/qt-5/qobject.html#connect

        QEventLoop loop;
        if (connect(this, &Cli::migrateDone, &loop, &QEventLoop::quit) == nullptr) {
            qCritical() << "failed to waiting for reply";
            return -1;
        }
        loop.exec();
    }

    ret =
      this->notifier->notify(api::types::v1::InteractionRequest{ .summary = retMsg.toStdString() });
    if (!ret) {
        this->printer.printReply({ .code = retCode, .message = retMsg.toStdString() });
        return -1;
    }

    return retCode;
}

std::vector<std::string>
Cli::filePathMapping(const std::vector<std::string> &command) const noexcept
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
            const auto file = options.filePath;

            if (file.empty()) {
                continue;
            }

            targetHostPath = LINGLONG_HOST_PATH + file;
            execArgs.push_back(targetHostPath);
            continue;
        }

        if (arg == "%%u") {
            const auto url = QString::fromStdString(options.fileUrl);

            if (options.fileUrl.empty()) {
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
                                     const std::string &type) noexcept
{
    // if type is all, do nothing, return tier of all packages.
    if (type == "all") {
        return;
    }

    std::vector<api::types::v1::PackageInfoV2> temp;

    // if type is runtime or app, return tier of specific type.
    for (const auto &info : list) {
        if (info.kind == type) {
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
