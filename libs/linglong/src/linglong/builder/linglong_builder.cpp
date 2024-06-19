/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/file.h"
#include "linglong/builder/printer.h"
#include "linglong/package/architecture.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/uab_packager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/packageinfo_handler.h"
#include "source_fetcher.h"

#include <nlohmann/json.hpp>
#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/wait.h>

namespace linglong::builder {

namespace {

utils::error::Result<package::Reference>
currentReference(const api::types::v1::BuilderProject &project)
{
    LINGLONG_TRACE("get current project reference");

    auto version = package::Version::parse(QString::fromStdString(project.package.version));
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
             const QDir &cacheDir,
             const QDir &destination,
             const api::types::v1::BuilderConfig &cfg) noexcept
{
    LINGLONG_TRACE("fetch sources to " + destination.absolutePath());

    for (int pos = 0; pos < sources.size(); ++pos) {
        printReplacedText(QString("%1%2%3%4")
                            .arg("Source " + QString::number(pos), -20)
                            .arg(QString::fromStdString(sources.at(pos).kind), -15)
                            .arg(QString::fromStdString(*(sources.at(pos).url)), -75)
                            .arg("downloading ...")
                            .toStdString(),
                          2);
        SourceFetcher sf(sources.at(pos), cfg, cacheDir);
        auto result = sf.fetch(QDir(destination));
        if (!result) {
            return LINGLONG_ERR(result);
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg("Source " + QString::number(pos), -20)
                            .arg(QString::fromStdString(sources.at(pos).kind), -15)
                            .arg(QString::fromStdString(*(sources.at(pos).url)), -75)
                            .arg("complete\n")
                            .toStdString(),
                          2);
    }

    return LINGLONG_OK;
}

utils::error::Result<package::Reference> pullDependency(const package::FuzzyReference &fuzzyRef,
                                                        repo::OSTreeRepo &repo,
                                                        bool develop,
                                                        bool onlyLocal) noexcept
{
    LINGLONG_TRACE("pull " + fuzzyRef.toString());

    if (onlyLocal) {
        auto ref =
          repo.clearReference(fuzzyRef, { .forceRemote = false, .fallbackToRemote = false });
        if (!ref) {
            return LINGLONG_ERR(ref);
        }
        return *ref;
    }
    auto ref = repo.clearReference(fuzzyRef, { .forceRemote = true, .fallbackToRemote = false });
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    // 如果依赖已存在，则直接使用
    auto baseLayerDir = repo.getLayerDir(*ref, develop);
    if (baseLayerDir) {
        return *ref;
    }

    auto taskID = QUuid::createUuid();
    auto taskPtr = std::make_shared<service::InstallTask>(taskID);

    auto partChanged =
      [&ref, develop](QString, QString percentage, QString, service::InstallTask::Status) {
          printReplacedText(QString("%1%2%3%4 %5")
                              .arg(ref->id, -25)
                              .arg(ref->version.toString(), -15)
                              .arg(develop ? "develop" : "binary", -15)
                              .arg("downloading")
                              .arg(percentage)
                              .toStdString(),
                            2);
      };
    QObject::connect(taskPtr.get(), &service::InstallTask::PartChanged, partChanged);
    repo.pull(taskPtr, *ref, develop);
    if (taskPtr->currentStatus() == service::InstallTask::Status::Failed) {
        return LINGLONG_ERR("pull " + ref->toString() + " failed",
                            std::move(taskPtr->currentError()));
    }
    return *ref;
}

utils::error::Result<package::Reference> pullDependency(const QString &fuzzyRefStr,
                                                        repo::OSTreeRepo &repo,
                                                        bool develop,
                                                        bool onlyLocal) noexcept
{
    LINGLONG_TRACE("pull " + fuzzyRefStr);
    auto fuzzyRef = package::FuzzyReference::parse(fuzzyRefStr);
    if (!fuzzyRef) {
        return LINGLONG_ERR(fuzzyRef);
    }

    return pullDependency(*fuzzyRef, repo, develop, onlyLocal);
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

// 拆分develop和binary的文件
utils::error::Result<void> Builder::splitDevelop(QDir developOutput,
                                                 QDir binaryOutput,
                                                 QString prefix)
{
    LINGLONG_TRACE("split layers file");
    const QString installFilename =
      QString("%1.install").arg(QString::fromStdString(project.package.id));
    const QString src = developOutput.absolutePath();
    const QString dest = binaryOutput.absolutePath();
    // get install file rule
    QStringList installRules;

    // if ${PROJECT_ROOT}/${appid}.install is not exist, copy all files
    const auto installRulePath = workingDir.filePath(installFilename);
    if (QFileInfo(installRulePath).exists()) {
        QFile configFile(installRulePath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            return LINGLONG_ERR("open file", configFile);
        }
        installRules.append(QString(configFile.readAll()).split('\n'));
        // remove empty or duplicate lines
        installRules.removeAll("");
        installRules.removeDuplicates();
    } else {
        qDebug() << "generate install list from " << src;
        QDirIterator it(src,
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            auto filepath = it.filePath();
            qDebug() << filepath;
            // $PROJECT_ROOT/.../files to /opt/apps/${appid}
            // $PROJECT_ROOT/ to /runtime/
            filepath.replace(0, src.length(), prefix);
            installRules.append(filepath);
        }
    }
    // 复制目录、文件和超链接
    auto copyFile = [&](const int &process,
                        const QFileInfo &info,
                        const QString &dstPath) -> utils::error::Result<void> {
        LINGLONG_TRACE("copy file");
        if (info.isDir()) {
            qDebug() << QString("process: %1%").arg(process) << "matched dir"
                     << info.absoluteFilePath();
            QDir().mkpath(dstPath);
            return LINGLONG_OK;
        }
        if (info.isSymLink()) {
            qDebug() << QString("process: %1%").arg(process) << "matched symlinks"
                     << info.absoluteFilePath();
            char buf[PATH_MAX];
            // qt的readlin无法区分相对链接还是绝对链接，所以用c库的readlink
            auto size = readlink(info.filePath().toStdString().c_str(), buf, sizeof(buf) - 1);
            if (size == -1) {
                qCritical() << "readlink failed! " << info.filePath();
                return LINGLONG_ERR("readlink failed!");
            }
            buf[size] = '\0';
            QString linkpath(buf);
            qDebug() << "link" << linkpath << "to" << dstPath;
            QFile file(linkpath);
            if (!file.link(dstPath))
                return LINGLONG_ERR("link file failed, relative path", file);
            return LINGLONG_OK;
        }
        // 链接也是文件，isFile要放到isSymLink后面
        if (info.isFile()) {
            qDebug() << QString("process: %1%").arg(process) << "matched file"
                     << info.absoluteFilePath();
            QDir().mkpath(info.path().replace(src, dest));
            QFile file(info.absoluteFilePath());
            if (!file.copy(dstPath))
                return LINGLONG_ERR("copy file", file);
            return LINGLONG_OK;
        }
        return LINGLONG_ERR(QString("unknown file type %1").arg(info.path()));
    };

    auto ruleIndex = 0;
    for (auto rule : installRules) {
        // 计算进度
        ruleIndex++;
        auto process = ruleIndex * 100 / installRules.length();
        // /opt/apps/${appid} to $PROJECT_ROOT/.../files
        // /runtime/ to $PROJECT_ROOT/
        rule.replace(0, prefix.length(), src);
        // 如果不以^符号开头，当作普通路径使用
        if (!rule.startsWith("^")) {
            QFileInfo info(rule);
            // 链接指向的文件如果不存在，info.exists会返回false
            // 所以要先判断文件是否是链接
            if (info.isSymLink() || info.exists()) {
                const QString dstPath = info.absoluteFilePath().replace(src, dest);
                auto ret = copyFile(process, info, dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            } else {
                qWarning() << "missing file" << rule;
            }
            continue;
        }
        // convert prefix in container to real path in host
        QRegularExpression re(rule);
        // reverse files in src
        QDirIterator it(src,
                        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (re.match(it.fileInfo().absoluteFilePath()).hasMatch()) {
                const QString dstPath = it.fileInfo().absoluteFilePath().replace(src, dest);
                auto ret = copyFile(process, it.fileInfo(), dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            }
        }
    }
    for (auto dir : { developOutput, binaryOutput }) {
        // save all installed file path to ${appid}.install
        const auto installRulePath = dir.filePath("../" + installFilename);
        QFile configFile(installRulePath);
        if (!configFile.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR("open file", configFile);
        }
        if (configFile.write(installRules.join('\n').toUtf8()) < 0) {
            return LINGLONG_ERR("write file", configFile);
        }
        configFile.close();
    }
    return LINGLONG_OK;
}

utils::error::Result<void> Builder::build(const QStringList &args) noexcept
{
    LINGLONG_TRACE(
      QString("build project %1").arg(this->workingDir.absoluteFilePath("linglong.yaml")));

    auto arch = package::Architecture::parse(QSysInfo::currentCpuArchitecture());
    if (!arch) {
        return LINGLONG_ERR(arch);
    }
    printMessage("[Build Target]");
    printMessage(this->project.package.id, 2);
    printMessage("[Project Info]");
    printMessage("Package Name: " + this->project.package.name, 2);
    printMessage("Version: " + this->project.package.version, 2);
    printMessage("Package Type: " + this->project.package.kind, 2);
    printMessage("Build Arch: " + arch->toString().toStdString(), 2);

    auto repoCfg = this->repo.getConfig();
    printMessage("[Current Repo]");
    printMessage("Name: " + repoCfg.defaultRepo, 2);
    printMessage("Url: " + repoCfg.repos.at(repoCfg.defaultRepo), 2);

    this->workingDir.mkdir("linglong");

    if (this->project.sources && !cfg.skipFetchSource) {
        printMessage("[Processing Sources]");
        printMessage(QString("%1%2%3%4")
                       .arg("Name", -20)
                       .arg("Type", -15)
                       .arg("Url", -75)
                       .arg("Status")
                       .toStdString(),
                     2);
        auto fetchCacheDir = this->workingDir.absoluteFilePath("linglong/cache");
        if (!qgetenv("LINGLONG_FETCH_CACHE").isEmpty()) {
            fetchCacheDir = qgetenv("LINGLONG_FETCH_CACHE");
        }
        auto result = fetchSources(*this->project.sources,
                                   fetchCacheDir,
                                   this->workingDir.absoluteFilePath("linglong/sources"),
                                   this->cfg);
        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    printMessage("[Processing Dependency]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", -25)
                   .arg("Version", -15)
                   .arg("Module", -15)
                   .arg("Status")
                   .toStdString(),
                 2);
    std::optional<package::Reference> runtime;
    std::optional<package::FuzzyReference> fuzzyRuntime;
    QString runtimeLayerDir;
    if (this->project.runtime) {
        auto fuzzyRef =
          package::FuzzyReference::parse(QString::fromStdString(*this->project.runtime));
        if (!fuzzyRef) {
            return LINGLONG_ERR(fuzzyRef);
        }

        fuzzyRuntime = *fuzzyRef;
        auto ref = pullDependency(*fuzzyRuntime,
                                  this->repo,
                                  true,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
        if (!ref) {
            return LINGLONG_ERR("pull runtime", ref);
        }
        runtime = *ref;
        auto ret = this->repo.getLayerDir(*runtime, true);
        if (!ret.has_value()) {
            return LINGLONG_ERR("get runtime layer dir", ret);
        }
        runtimeLayerDir = ret->absolutePath();
        printReplacedText(QString("%1%2%3%4")
                            .arg(runtime->id, -25)
                            .arg(runtime->version.toString(), -15)
                            .arg("develop", -15)
                            .arg("complete\n")
                            .toStdString(),
                          2);
        qDebug() << "pull runtime success" << runtime->toString();
    }

    auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(this->project.base));
    if (!fuzzyBase) {
        return LINGLONG_ERR(fuzzyBase);
    }
    auto base = pullDependency(*fuzzyBase,
                               this->repo,
                               true,
                               cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
    if (!base) {
        return LINGLONG_ERR("pull base", base);
    }
    auto baseLayerDir = this->repo.getLayerDir(*base, true);
    if (!baseLayerDir) {
        return LINGLONG_ERR(baseLayerDir);
    }
    printReplacedText(QString("%1%2%3%4")
                        .arg(base->id, -25)
                        .arg(base->version.toString(), -15)
                        .arg("develop", -15)
                        .arg("complete\n")
                        .toStdString(),
                      2);
    qDebug() << "pull base success" << base->toString();

    if (cfg.skipRunContainer) {
        return LINGLONG_OK;
    }

    QFile entry = this->workingDir.absoluteFilePath("linglong/entry.sh");
    if (entry.exists() && !entry.remove()) {
        return LINGLONG_ERR(entry);
    }

    if (!entry.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(entry);
    }
    // FIXME: check bytes writed.
    entry.write("#!/bin/bash\n");
    entry.write("set -e\n\n");
    entry.write("# This file is generated by `build` in linglong.yaml\n");
    entry.write("# DO NOT EDIT IT\n\n");
    entry.write(project.build.c_str());
    entry.close();
    if (entry.error() != QFile::NoError) {
        return LINGLONG_ERR(entry);
    }

    if (!entry.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ExeOwner)) {
        return LINGLONG_ERR("set file permission error:", entry);
    }
    qDebug() << "generated entry.sh success";

    // clean output
    QDir(this->workingDir.absoluteFilePath("linglong/output")).removeRecursively();

    QDir developOutput = this->workingDir.absoluteFilePath("linglong/output/develop/files");
    if (!developOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + developOutput.absolutePath() + ": failed.");
    }
    qDebug() << "create develop output success";

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto opts = runtime::ContainerOptions{
        .appID = QString::fromStdString(this->project.package.id),
        .containerID = ("linglong-builder-" + ref->toString() + QUuid::createUuid().toString())
                         .toUtf8()
                         .toBase64(),
        .baseDir = *baseLayerDir,
        .appDir = {},
        .patches = {},
        .mounts = {},
    };
    if (!runtimeLayerDir.isEmpty()) {
        opts.runtimeDir = runtimeLayerDir;
    }
    // 构建安装路径
    QString installPrefix = "/runtime";
    if (this->project.package.kind != "runtime") {
        installPrefix = QString::fromStdString("/opt/apps/" + this->project.package.id + "/files");
    }
    opts.mounts.push_back({
      .destination = installPrefix.toStdString(),
      .gidMappings = {},
      .options = { { "rbind", "rw" } },
      .source = developOutput.absolutePath().toStdString(),
      .type = "bind",
      .uidMappings = {},
    });
    if (QDir("/usr/libexec/linglong/builder/helper").exists()) {
        opts.mounts.push_back({
          .destination = "/usr/libexec/linglong/builder/helper",
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = "/usr/libexec/linglong/builder/helper",
          .type = "bind",
          .uidMappings = {},
        });
    }
    opts.mounts.push_back({
      .destination = "/project",
      .gidMappings = {},
      .options = { { "rbind", "rw" } },
      .source = this->workingDir.absolutePath().toStdString(),
      .type = "bind",
      .uidMappings = {},
    });

    opts.masks.emplace_back("/project/linglong/output");

    auto container = this->containerBuilder.create(opts);
    if (!container) {
        return LINGLONG_ERR(container);
    }
    qDebug() << "create container success";

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
        .cwd = "/project",
        .env = { {
          "PREFIX=" + installPrefix.toStdString(),
          "TRIPLET=" + arch->getTriplet().toStdString(),
        } },
        .ioPriority = {},
        .noNewPrivileges = true,
        .oomScoreAdj = {},
        .rlimits = {},
        .scheduler = {},
        .selinuxLabel = {},
        .terminal = true,
    };
    printMessage("[Start Build]");
    auto result = (*container)->run(process);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    qDebug() << "run container success";
    if (cfg.skipCommitOutput) {
        return LINGLONG_OK;
    }

    printMessage("[Commit Contents]");
    QDir binaryOutput = this->workingDir.absoluteFilePath("linglong/output/binary/files");
    if (!binaryOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + binaryOutput.absolutePath() + ": failed.");
    }
    qDebug() << "create binary output success";

    // generate application's configure file
    auto scriptFile = QString(LINGLONG_LIBEXEC_DIR) + "/app-conf-generator";
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        qWarning() << "Dumping app-conf-generator from qrc...";
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath("app-conf-generator");
        QFile::copy(":/scripts/app-conf-generator", scriptFile);
    }
    auto output = utils::command::Exec(
      "bash",
      QStringList() << scriptFile << QString::fromStdString(this->project.package.id)
                    << developOutput.absolutePath());
    if (!output) {
        return LINGLONG_ERR(output);
    }

    auto ret =
      splitDevelop(developOutput.absolutePath(), binaryOutput.absolutePath(), installPrefix);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    if (this->project.package.kind != "runtime") {
        QDir binaryEntries = this->workingDir.absoluteFilePath("linglong/output/binary/entries");
        QDir developEntries = this->workingDir.absoluteFilePath("linglong/output/develop/entries");
        if (!binaryEntries.mkpath(".")) {
            return LINGLONG_ERR("make path " + binaryEntries.absolutePath() + ": failed.");
        }
        if (!developEntries.mkpath(".")) {
            return LINGLONG_ERR("make path " + developEntries.absolutePath() + ": failed.");
        }
        if (!QFile::link("../files/share", binaryEntries.absoluteFilePath("share"))) {
            return LINGLONG_ERR("link entries share to files share: failed");
        }
        if (!binaryOutput.mkpath("share/systemd")) {
            return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
        }
        if (!QFile::link("../../lib/systemd/user",
                         binaryOutput.absoluteFilePath("share/systemd/user"))) {
            return LINGLONG_ERR("link systemd user service to files/share/systemd/user: failed");
        }
        if (!QFile::link("../files/share", developEntries.absoluteFilePath("share"))) {
            return LINGLONG_ERR("link entries share to files share: failed");
        }
        if (!developOutput.mkpath("share/systemd")) {
            return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
        }
        if (!QFile::link("../../lib/systemd/user",
                         developOutput.absoluteFilePath("share/systemd/user"))) {
            return LINGLONG_ERR("link systemd user service to files/share/systemd/user: failed");
        }
        if (project.command.value_or(std::vector<std::string>{}).empty()) {
            return LINGLONG_ERR("command field is required, please specify!");
        }
    }

    // when the base version is likes 20.0.0.1, warning that it is a full version
    // if the base version is likes 20.0.0, we should also write 20.0.0 to info.json
    if (fuzzyBase->version->tweak) {
        qWarning() << fuzzyBase->toString() << "is set a full version.";
    } else {
        base->version.tweak = std::nullopt;
    }

    auto info = api::types::v1::PackageInfoV2{
        .arch = { QSysInfo::currentCpuArchitecture().toStdString() },
        .base = base->toString().toStdString(),
        .channel = ref->channel.toStdString(),
        .command = project.command,
        .description = this->project.package.description,
        .id = this->project.package.id,
        .kind = this->project.package.kind,
        .packageInfoV2Module = "binary",
        .name = this->project.package.name,
        .permissions = this->project.permissions,
        .runtime = {},
        .schemaVersion = PACKAGE_INFO_VERSION,
        .size = static_cast<int64_t>(util::sizeOfDir(binaryOutput.absoluteFilePath(".."))),
        .version = this->project.package.version,
    };

    if (runtime) {
        // the runtime version is same as base.
        if (fuzzyRuntime->version->tweak) {
            qWarning() << fuzzyRuntime->toString() << "is set a full version.";
        } else {
            runtime->version.tweak = std::nullopt;
        }
        info.runtime = runtime->toString().toStdString();
    }

    QFile infoFile = binaryOutput.absoluteFilePath("../info.json");
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.write(nlohmann::json(info).dump().c_str());
    if (infoFile.error() != QFile::NoError) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.close();

    info.packageInfoV2Module = "develop";
    info.size = util::sizeOfDir(developOutput.absoluteFilePath(".."));

    infoFile.setFileName(developOutput.absoluteFilePath("../info.json"));
    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return LINGLONG_ERR(infoFile);
    }

    infoFile.write(nlohmann::json(info).dump().c_str());
    if (infoFile.error() != QFile::NoError) {
        return LINGLONG_ERR(infoFile);
    }
    infoFile.close();
    package::LayerDir binaryOutputLayerDir = binaryOutput.absoluteFilePath("..");
    result = this->repo.remove(*ref);
    if (!result) {
        qWarning() << "remove" << ref->toString() << result.error().message();
    }
    result = this->repo.importLayerDir(binaryOutputLayerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    package::LayerDir developOutputLayerDir = developOutput.absoluteFilePath("..");
    result = this->repo.remove(*ref, true);
    if (!result) {
        qWarning() << "remove" << ref->toString() << result.error().message();
    }
    result = this->repo.importLayerDir(developOutputLayerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    printMessage("Successfully build " + this->project.package.id);
    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportUAB(const QString &destination, const UABOption &option)
{
    LINGLONG_TRACE("export uab file");

    QDir destDir(destination);
    if (!destDir.mkpath(".")) {
        return LINGLONG_ERR("mkpath " + destination + ": failed");
    }

    package::UABPackager packager{ destDir };

    if (!option.iconPath.isEmpty()) {
        if (auto ret = packager.setIcon(option.iconPath); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    auto baseRef = pullDependency(QString::fromStdString(this->project.base),
                                  this->repo,
                                  false,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }
    auto baseDir = this->repo.getLayerDir(*baseRef, false);
    if (!baseDir) {
        return LINGLONG_ERR(baseDir);
    }
    packager.appendLayer(*baseDir);

    if (this->project.runtime) {
        auto ref = pullDependency(QString::fromStdString(*this->project.runtime),
                                  this->repo,
                                  false,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }
        auto runtimeDir = this->repo.getLayerDir(*ref, false);
        if (!runtimeDir) {
            return LINGLONG_ERR(runtimeDir);
        }
        packager.appendLayer(*runtimeDir);
    }

    auto curRef = currentReference(this->project);
    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    auto appDir = this->repo.getLayerDir(*curRef);
    if (!appDir) {
        return LINGLONG_ERR(appDir);
    }
    packager.appendLayer(*appDir);

    auto uabFile = curRef->id + ".uab";
    if (auto ret = packager.pack(uabFile); !ret) {
        return LINGLONG_ERR(ret);
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

    auto binaryLayerDir = this->repo.getLayerDir(*ref);
    if (!binaryLayerDir) {
        return LINGLONG_ERR(binaryLayerDir);
    }
    const auto binaryLayerPath = QString("%1/%2_%3_%4_%5.layer")
                                   .arg(destDir.absolutePath(),
                                        ref->id,
                                        ref->version.toString(),
                                        ref->arch.toString(),
                                        "binary");

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

    auto binaryLayer = pkger.pack(*binaryLayerDir, binaryLayerPath);
    if (!binaryLayer) {
        return LINGLONG_ERR(binaryLayer);
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
    if (destDir.exists()) {
        return LINGLONG_ERR(destination + " already exists");
    }

    package::LayerPackager pkg;
    auto layerDir = pkg.unpack(*(*layerFile));
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto output = utils::command::Exec("cp",
                                       QStringList() << "-r" << layerDir->absolutePath()
                                                     << destDir.absolutePath());
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

    if (pushWithDevel) {
        result = repo.push(*ref, true);

        if (!result) {
            return LINGLONG_ERR(result);
        }
    }

    result = repo.push(*ref);
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

    auto curRef = currentReference(this->project);
    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    auto curDir = this->repo.getLayerDir(*curRef);
    if (!curDir) {
        return LINGLONG_ERR(curDir);
    }

    auto info = curDir->info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    auto options = runtime::ContainerOptions{
        .appID = curRef->id,
        .containerID =
          (curRef->toString() + "-" + QUuid::createUuid().toString()).toUtf8().toBase64(),
        .runtimeDir = {},
        .baseDir = {},
        .appDir = {},
        .patches = {},
        .mounts = {},
    };

    auto baseRef = pullDependency(QString::fromStdString(this->project.base),
                                  this->repo,
                                  false,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }
    auto baseDir = this->repo.getLayerDir(*baseRef, false);
    if (!baseDir) {
        return LINGLONG_ERR(baseDir);
    }
    options.baseDir = QDir(baseDir->absolutePath());

    if (this->project.runtime) {
        auto ref = pullDependency(QString::fromStdString(*this->project.runtime),
                                  this->repo,
                                  false,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
        if (!ref) {
            return LINGLONG_ERR(ref);
        }
        auto dir = this->repo.getLayerDir(*ref, false);
        if (!dir) {
            return LINGLONG_ERR(dir);
        }
        options.runtimeDir = QDir(dir->absolutePath());
    }

    if (this->project.package.kind == "runtime") {
        options.runtimeDir = QDir(curDir->absolutePath());
    }

    if (this->project.package.kind == "base") {
        options.baseDir = QDir(curDir->absolutePath());
    }

    if (this->project.package.kind == "app") {
        options.appDir = QDir(curDir->absolutePath());
    }

    std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
    auto bindMount =
      [&applicationMounts](const api::types::v1::ApplicationConfigurationPermissionsBind &bind) {
          applicationMounts.push_back(ocppi::runtime::config::types::Mount{
            .destination = bind.destination,
            .options = { { "rbind" } },
            .source = bind.source,
            .type = "bind",
          });
      };
    auto bindInnerMount =
      [&applicationMounts](
        const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
          applicationMounts.push_back(ocppi::runtime::config::types::Mount{
            .destination = bind.destination,
            .options = { { "rbind" } },
            .source = "rootfs" + bind.source,
            .type = "bind",
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
              std::filesystem::path{ curDir->absolutePath().toStdString() };
            std::for_each(innerBinds->cbegin(), innerBinds->cend(), bindInnerMount);
        }
    }

    options.mounts = std::move(applicationMounts);

    auto container = this->containerBuilder.create(options);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process p;

    if (!args.isEmpty()) {
        p.args = std::vector<std::string>{};
        for (const auto &arg : args) {
            p.args->push_back(arg.toStdString());
        }
    } else {
        p.args = this->project.command;
        if (!p.args) {
            p.args = std::vector<std::string>{ "bash" };
        }
    }

    auto result = container->data()->run(p);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
