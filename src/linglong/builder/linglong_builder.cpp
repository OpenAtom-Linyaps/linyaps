/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/file.h"
#include "linglong/package/layer_packager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/command/ocppi-helper.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/xdg/desktop_entry.h"
#include "nlohmann/json_fwd.hpp"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/ContainerID.hpp"
#include "ocppi/runtime/config/types/Hook.hpp"
#include "ocppi/runtime/config/types/Hooks.hpp"
#include "source_fetcher.h"

#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>

#include <fstream>
#include <optional>

#include <sys/socket.h>
#include <sys/wait.h>

namespace linglong::builder {

namespace {

utils::error::Result<package::Reference>
currentReference(const api::types::v1::BuilderProject &project)
{
    LINGLONG_TRACE("get current project reference");

    auto version = package::Version::parse(QString::fromStdString(project.version));
    if (!version) {
        return LINGLONG_ERR(version);
    }

    auto architecture = package::Architecture::parse(QSysInfo::currentCpuArchitecture());
    if (project.package.architecture) {
        architecture =
          package::Architecture::parse(QString::fromStdString(*project.package.architecture));
    }
    if (!architecture) {
        return LINGLONG_ERR(architecture);
    }

    auto ref = package::Reference::create("main",
                                          QString::fromStdString(project.package.id),
                                          *version,
                                          *architecture);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    return ref;
}

utils::error::Result<void>
fetchSources(const std::vector<api::types::v1::BuilderProjectSource> &sources,
             const QDir &destination,
             const api::types::v1::BuilderConfig &cfg) noexcept
{
    LINGLONG_TRACE("fetch sources to " + destination.absolutePath());

    for (const auto &source : sources) {
        SourceFetcher sf(source, cfg);
        auto result = sf.fetch(destination);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<package::Reference> pullDependency(QString fuzzyRefStr,
                                                        repo::OSTreeRepo &repo) noexcept
{
    LINGLONG_TRACE("pull " + fuzzyRefStr);

    auto fuzzyRef = package::FuzzyReference::parse(fuzzyRefStr);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef);
    }

    auto ref = repo.clearReference(*fuzzyRef, { .forceRemote = true, .fallbackToRemote = false });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto taskID = QUuid::createUuid();
    auto taskPtr = std::make_shared<service::InstallTask>(taskID);
    auto taskChanged =
      [](QString taskID, QString percentage, QString message, service::InstallTask::Status status) {
          qInfo().noquote() << QString("%1%%").arg(percentage) << message;
      };
    QObject::connect(taskPtr.get(), &service::InstallTask::TaskChanged, taskChanged);
    repo.pull(taskPtr, *ref, true);
    if (taskPtr->currentStatus() != service::InstallTask::Status::Success) {
        return LINGLONG_ERR("pull " + ref->toString() + " failed");
    }

    return *ref;
}

} // namespace

Builder::Builder(const api::types::v1::BuilderProject &project,
                 QDir workingDir,
                 repo::OSTreeRepo &repo,
                 runtime::ContainerBuilder &containerBuilder,
                 const api::types::v1::BuilderConfig &cfg)
    : repo(repo)
    , workingDir(workingDir)
    , project(project)
    , containerBuilder(containerBuilder)
    , cfg(cfg)
{
}

auto Builder::getConfig() const noexcept -> api::types::v1::BuilderConfig
{
    return this->cfg;
}

void Builder::setConfig(const api::types::v1::BuilderConfig &cfg) noexcept
{
    if (this->cfg.repo != cfg.repo) {
        qCritical() << "update ostree repo path of builder is not supported.";
        std::abort();
    }

    this->cfg = cfg;
}

utils::error::Result<void> Builder::build(const QStringList &args) noexcept
{
    LINGLONG_TRACE(
      QString("build project %1").arg(this->workingDir.absoluteFilePath("linglong.yaml")));

    if (this->project.sources) {
        auto result = fetchSources(*this->project.sources,
                                   this->workingDir.absoluteFilePath("linglong/sources"),
                                   this->cfg);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    std::optional<package::Reference> runtime;

    if (this->project.runtime) {
        auto ref = pullDependency(QString::fromStdString(*this->project.runtime), this->repo);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }

        runtime = *ref;
    }

    auto base = pullDependency(QString::fromStdString(this->project.base), this->repo);
    if (!base) {
        return LINGLONG_ERR(base);
    }

    QFile entry = this->workingDir.absoluteFilePath("linglong/entry.sh");
    if (!entry.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return LINGLONG_ERR(entry);
    }
    // FIXME: check bytes writed.
    entry.write("#!/bin/bash\n\n# This file is generated by `build` in linglong.yaml\n# DO NOT "
                "EDIT IT\n\n");
    entry.write(project.build.c_str());
    if (entry.error() != QFile::NoError) {
        return LINGLONG_ERR(entry);
    }

    QDir developOutput = this->workingDir.absoluteFilePath("linglong/output/develop/files");
    QDir(developOutput.absoluteFilePath("..")).removeRecursively();
    if (!developOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + developOutput.absolutePath() + ": failed.");
    }

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto opts = runtime::ContainerOptions{
        .appID = QString::fromStdString(this->project.package.id),
        .containerID = "linglong-builder-" + ref->toString(), // FIXME
        .runtimeDir = {},
        .baseDir = *this->repo.getLayerDir(*base),
        .appDir = {},
        .patches = {},
        .mounts = {},
    };

    if (this->project.package.kind == "runtime") {
        opts.mounts.push_back({
          .destination = "/runtime",
          .gidMappings = {},
          .options = { { "rbind", "rw" } },
          .source = developOutput.absolutePath().toStdString(),
          .type = "bind",
          .uidMappings = {},
        });
    } else {
        opts.mounts.push_back({
          .destination = "/opt/apps/" + this->project.package.id + "/files",
          .gidMappings = {},
          .options = { { "rbind", "rw" } },
          .source = developOutput.absolutePath().toStdString(),
          .type = "bind",
          .uidMappings = {},
        });
    }

    opts.mounts.push_back({
      .destination = "/usr/libexec/linglong/builder/helper",
      .gidMappings = {},
      .options = { { "rbind", "ro" } },
      .source = "/usr/libexec/linglong/builder/helper",
      .type = "bind",
      .uidMappings = {},
    });

    opts.mounts.push_back({
      .destination = "/source",
      .gidMappings = {},
      .options = { { "rbind", "rw" } },
      .source = this->workingDir.absolutePath().toStdString(),
      .type = "bind",
      .uidMappings = {},
    });

    opts.masks.emplace_back("/source/linglong");

    auto container = this->containerBuilder.create(opts);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    auto arguments = std::vector<std::string>{};
    for (const auto &arg : args) {
        arguments.push_back(arg.toStdString());
    }

    auto process = ocppi::runtime::config::types::Process{
        .apparmorProfile = {},
        .args = arguments,
        .capabilities = {},
        .commandLine = {},
        .consoleSize = {},
        .cwd = "/source",
        .env = {}, // FIXME: add environments later.
        .ioPriority = {},
        .noNewPrivileges = true,
        .oomScoreAdj = {},
        .rlimits = {},
        .scheduler = {},
        .selinuxLabel = {},
        .terminal = true,
        .user = { {
          .additionalGids = {},
          .gid = getgid(),
          .uid = getuid(),
          .umask = {},
          .username = {},
        } },
    };

    auto result = (*container)->run(process);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    QDir runtimeOutput = this->workingDir.absoluteFilePath("linglong/output/runtime/files");
    QDir(runtimeOutput.absoluteFilePath("..")).removeRecursively();
    if (!runtimeOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + runtimeOutput.absolutePath() + ": failed.");
    }

    // TODO

    QProcess p;
    p.setProgram("cp");
    p.setArguments({ "-a", developOutput.absolutePath(), runtimeOutput.absolutePath() });
    p.start();

    if (!p.waitForFinished(-1)) {
        return LINGLONG_ERR("copy files to runtime module", p.errorString());
    }

    // FIXME: not just simply link, we should modify something.
    if (!QFile::link("../files/share", runtimeOutput.absoluteFilePath("../entries"))) {
        return LINGLONG_ERR("link entries to files share: failed");
    }
    if (!runtimeOutput.mkpath("files/share/systemd/user")) {
        return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
    }
    if (!QFile::link("../../lib/systemd/user",
                     runtimeOutput.absoluteFilePath("files/share/systemd/user"))) {
        return LINGLONG_ERR("link systemd user service to files/share/systemd/user: failed");
    }
    if (!QFile::link("../files/share", developOutput.absoluteFilePath("../entries"))) {
        return LINGLONG_ERR("link entries to files share: failed");
    }
    if (!developOutput.mkpath("files/share/systemd/user")) {
        return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
    }
    if (!QFile::link("../../lib/systemd/user",
                     developOutput.absoluteFilePath("files/share/systemd/user"))) {
        return LINGLONG_ERR("link systemd user service to files/share/systemd/user: failed");
    }

    auto info = api::types::v1::PackageInfo{
        .appID = this->project.package.id,
        .arch = QSysInfo::currentCpuArchitecture().toStdString(),
        .base = base->toString().toStdString(),
        .channel = ref->channel.toStdString(),
        .description = this->project.package.description,
        .kind = this->project.package.kind,
        .packageInfoModule = "runtime",
        .name = this->project.package.name,
        .permissions = {},
        .runtime = {},
        .size = 0,
        .version = this->project.package.version,
    };

    if (runtime) {
        info.runtime = runtime->toString().toStdString();
    }

    QFile infoFile = runtimeOutput.absoluteFilePath("../info.json");
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.write(nlohmann::json(info).dump().c_str());
    if (infoFile.error() != QFile::NoError) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.close();

    info.packageInfoModule = "devel";

    infoFile.setFileName(developOutput.absoluteFilePath("../info.json"));
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.write(nlohmann::json(info).dump().c_str());
    if (infoFile.error() != QFile::NoError) {
        return LINGLONG_ERR(infoFile);
    }
    infoFile.close();

    package::LayerDir runtimeLayerDir = runtimeOutput.absoluteFilePath("..");
    this->repo.remove(*ref);
    result = this->repo.importLayerDir(runtimeLayerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    package::LayerDir developLayerDir = developOutput.absoluteFilePath("..");
    this->repo.remove(*ref, true);
    result = this->repo.importLayerDir(developLayerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportLayer(const QString &destination)
{
    LINGLONG_TRACE("export layer file");

    QDir destDir(destination);
    destDir.mkpath(".");

    if (!destDir.exists()) {
        return LINGLONG_ERR("mkpath " + destination + ": failed");
    }

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto runtimeLayerDir = this->repo.getLayerDir(*ref);
    if (!runtimeLayerDir) {
        return LINGLONG_ERR(runtimeLayerDir);
    }
    const auto runtimeLayerPath = QString("%1/%2_%3_%4_%5.layer")
                                    .arg(destDir.absolutePath(),
                                         ref->id,
                                         ref->version.toString(),
                                         ref->arch.toString(),
                                         "runtime");

    auto developLayerDir = this->repo.getLayerDir(*ref, true);
    const auto develLayerPath = QString("%1/%2_%3_%4_%5.layer")
                                  .arg(destDir.absolutePath(),
                                       ref->id,
                                       ref->version.toString(),
                                       ref->arch.toString(),
                                       "develop");
    if (!developLayerDir) {
        return LINGLONG_ERR(developLayerDir);
    }

    package::LayerPackager pkger;

    auto runtimeLayer = pkger.pack(*runtimeLayerDir, runtimeLayerPath);
    if (!runtimeLayer) {
        return LINGLONG_ERR(runtimeLayer);
    }

    auto develLayer = pkger.pack(*developLayerDir, develLayerPath);
    if (!develLayer) {
        return LINGLONG_ERR(develLayer);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::extractLayer(const QString &layerPath,
                                                 const QString &destination)
{
    LINGLONG_TRACE("extract " + layerPath + " to " + destination);

    auto layerFile = package::LayerFile::New(layerPath);
    if (!layerFile) {
        return LINGLONG_ERR(layerFile);
    }

    QDir destDir = destination;
    if (!destDir.mkpath(".")) {
        return LINGLONG_ERR("mkpath " + destination);
    }

    package::LayerPackager pkg;
    auto layerDir = pkg.unpack(*(*layerFile));
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto output =
      utils::command::Exec("mv",
                           QStringList() << layerDir->absolutePath() << destDir.absolutePath());
    if (!output) {
        return LINGLONG_ERR(output);
    }
    return LINGLONG_OK;
}

linglong::utils::error::Result<void> Builder::push(bool pushWithDevel,
                                                   const QString &repoName,
                                                   const QString &repoUrl)
{
    LINGLONG_TRACE("push reference to remote repository");

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto cfg = this->repo.getConfig();
    auto oldCfg = cfg;

    auto _ = utils::finally::finally([this, &oldCfg]() {
        this->repo.setConfig(oldCfg);
    });

    if (repoName != "") {
        cfg.defaultRepo = repoName.toStdString();
    }
    if (repoUrl != "") {
        cfg.repos[cfg.defaultRepo] = repoUrl.toStdString();
    }

    auto result = this->repo.setConfig(cfg);

    if (!result) {
        return LINGLONG_ERR(result);
    }

    result = repo.push(*ref);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    if (!pushWithDevel) {
        return LINGLONG_OK;
    }

    result = repo.push(*ref, true);

    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::importLayer(const QString &path)
{
    LINGLONG_TRACE("import layer");

    auto layerFile = package::LayerFile::New(path);
    if (!layerFile) {
        return LINGLONG_ERR(layerFile);
    }

    package::LayerPackager pkg;

    auto layerDir = pkg.unpack(*(*layerFile));
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto result = this->repo.importLayerDir(*layerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::run(const QStringList &args)
{
    LINGLONG_TRACE("run application");

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto options = runtime::ContainerOptions{
        .appID = ref->id,
        .containerID = (ref->toString() + "-" + QUuid::createUuid().toString()).toUtf8().toBase64(),
        .runtimeDir = {},
        .baseDir = {},
        .appDir = {},
        .patches = {},
        .mounts = {},
    };

    ref = pullDependency(QString::fromStdString(this->project.base), this->repo);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    auto dir = this->repo.getLayerDir(*ref);
    if (!dir) {
        return LINGLONG_ERR(dir);
    }
    options.baseDir = QDir(dir->absoluteFilePath("files"));

    if (this->project.runtime) {
        ref = pullDependency(QString::fromStdString(*this->project.runtime), this->repo);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }
        dir = this->repo.getLayerDir(*ref);
        if (!dir) {
            return LINGLONG_ERR(dir);
        }
        options.runtimeDir = QDir(dir->absoluteFilePath("files"));
    }

    dir = this->repo.getLayerDir(*ref);
    if (!dir) {
        return LINGLONG_ERR(dir);
    }

    if (this->project.package.kind == "runtime") {
        options.runtimeDir = QDir(dir->absoluteFilePath("files"));
    }

    if (this->project.package.kind == "base") {
        options.baseDir = QDir(dir->absoluteFilePath("files"));
    }

    if (this->project.package.kind == "app") {
        options.appDir = QDir(dir->absoluteFilePath("files"));
    }

    auto container = this->containerBuilder.create(options);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process p;

    p.args = std::vector<std::string>{};
    for (const auto &arg : args) {
        p.args->push_back(arg.toStdString());
    }

    auto result = container->data()->run(p);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
