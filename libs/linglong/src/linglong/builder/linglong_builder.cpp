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

    auto architecture = package::Architecture::currentCPUArchitecture();
    if (project.package.architecture) {
        architecture = package::Architecture::parse(*project.package.architecture);
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

    for (decltype(sources.size()) pos = 0; pos < sources.size(); ++pos) {
        auto url = QString::fromStdString(*(sources.at(pos).url));
        if (url.length() > 75) {         // NOLINT
            url = "..." + url.right(70); // NOLINT
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg("Source " + QString::number(pos), -20)             // NOLINT
                            .arg(QString::fromStdString(sources.at(pos).kind), -15) // NOLINT
                            .arg(url, -75)                                          // NOLINT
                            .arg("downloading ...")
                            .toStdString(),
                          2);
        SourceFetcher fetcher(sources.at(pos), cfg, cacheDir);
        auto result = fetcher.fetch(QDir(destination));
        if (!result) {
            return LINGLONG_ERR(result);
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg("Source " + QString::number(pos), -20)             // NOLINT
                            .arg(QString::fromStdString(sources.at(pos).kind), -15) // NOLINT
                            .arg(url, -75)                                          // NOLINT
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

    auto tmpTask = service::PackageTask::createTemporaryTask();
    auto partChanged = [&ref, develop](const QString &percentage) {
        printReplacedText(QString("%1%2%3%4 %5")
                            .arg(ref->id, -25)                        // NOLINT
                            .arg(ref->version.toString(), -15)        // NOLINT
                            .arg(develop ? "develop" : "binary", -15) // NOLINT
                            .arg("downloading")
                            .arg(percentage)
                            .toStdString(),
                          2);
    };
    QObject::connect(&tmpTask, &service::PackageTask::PartChanged, partChanged);
    repo.pull(tmpTask, *ref, develop);
    if (tmpTask.state() == service::PackageTask::Failed) {
        return LINGLONG_ERR("pull " + ref->toString() + " failed", std::move(tmpTask).takeError());
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

// 拆分develop和binary的文件
utils::error::Result<void> splitDevelop(QString installFilepath,
                                        const QDir &developOutput,
                                        const QDir &binaryOutput,
                                        const QString &prefix,
                                        const std::function<void(int)> &handleProgress)
{
    LINGLONG_TRACE("split layers file");
    const QString src = developOutput.absolutePath();
    const QString dest = binaryOutput.absolutePath();
    // get install file rule
    QStringList installRules;

    // if ${PROJECT_ROOT}/${appid}.install is not exist, copy all files
    if (QFileInfo(installFilepath).exists()) {
        QFile configFile(installFilepath);
        if (!configFile.open(QIODevice::ReadOnly)) {
            return LINGLONG_ERR("open file", configFile);
        }
        installRules.append(QString(configFile.readAll()).split('\n'));
        // remove empty or duplicate lines
        installRules.removeAll("");
        installRules.removeDuplicates();
    } else {
        qDebug() << "generate install list from " << src;
        QDirIterator iter(src,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            iter.next();
            auto filepath = iter.filePath();
            qDebug() << filepath;
            // $PROJECT_ROOT/.../files to /opt/apps/${appid}
            // $PROJECT_ROOT/ to /runtime/
            filepath.replace(0, src.length(), prefix);
            installRules.append(filepath);
        }
    }
    // 复制目录、文件和超链接
    auto copyFile = [&](const int &percentage,
                        const QFileInfo &info,
                        const QString &dstPath) -> utils::error::Result<void> {
        LINGLONG_TRACE("copy file");
        if (info.isDir()) {
            qDebug() << QString("percentage: %1%").arg(percentage) << "matched dir"
                     << info.absoluteFilePath();
            QDir().mkpath(dstPath);
            return LINGLONG_OK;
        }
        if (info.isSymLink()) {
            qDebug() << QString("percentage: %1%").arg(percentage) << "matched symlinks"
                     << info.absoluteFilePath();
            std::array<char, PATH_MAX + 1> buf{};
            // qt的readlin无法区分相对链接还是绝对链接，所以用c库的readlink
            auto size = readlink(info.filePath().toStdString().c_str(), buf.data(), PATH_MAX);
            if (size == -1) {
                qCritical() << "readlink failed! " << info.filePath();
                return LINGLONG_ERR("readlink failed!");
            }

            QString linkpath(buf.data());
            qDebug() << "link" << linkpath << "to" << dstPath;
            QFile file(linkpath);
            if (!file.link(dstPath)) {
                return LINGLONG_ERR("link file failed, relative path", file);
            }
            return LINGLONG_OK;
        }
        // 链接也是文件，isFile要放到isSymLink后面
        if (info.isFile()) {
            qDebug() << QString("percentage: %1%").arg(percentage) << "matched file"
                     << info.absoluteFilePath();
            QDir().mkpath(info.path().replace(src, dest));
            QFile file(info.absoluteFilePath());
            if (!file.copy(dstPath)) {
                return LINGLONG_ERR("copy file", file);
            }
            return LINGLONG_OK;
        }
        return LINGLONG_ERR(QString("unknown file type %1").arg(info.path()));
    };

    auto ruleIndex = 0;
    for (auto rule : installRules) {
        // 计算进度
        ruleIndex++;
        auto percentage = ruleIndex * 100 / installRules.length(); // NOLINT
        // 统计进度
        if (handleProgress) {
            handleProgress(percentage);
        }
        // 跳过注释
        if (rule.startsWith("#")) {
            continue;
        }
        // 如果不以^符号开头，当作普通路径使用
        if (!rule.startsWith("^")) {
            // replace $prefix with $PROJECT_ROOT/output/$model/files
            rule.replace(0, prefix.length(), src);
            QFileInfo info(rule);
            // 链接指向的文件如果不存在，info.exists会返回false
            // 所以要先判断文件是否是链接
            if (info.isSymLink() || info.exists()) {
                const QString dstPath = info.absoluteFilePath().replace(src, dest);
                auto ret = copyFile(percentage, info, dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            } else {
                qWarning() << "missing file" << rule;
            }
            continue;
        }
        // replace $prefix with $PROJECT_ROOT/output/$model/files
        // TODO(wurongjie) 应该只替换一次, 避免路径包含多个prefix
        // 但不能使用 rule.replace(0, prefix.length), 会导致正则匹配错误
        rule.replace(prefix, src);
        // convert prefix in container to real path in host
        QRegularExpression regexp(rule);
        // reverse files in src
        QDirIterator iter(src,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            iter.next();
            if (regexp.match(iter.fileInfo().absoluteFilePath()).hasMatch()) {
                const QString dstPath = iter.fileInfo().absoluteFilePath().replace(src, dest);
                auto ret = copyFile(percentage, iter.fileInfo(), dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            }
        }
    }
    for (const auto &dir : { developOutput, binaryOutput }) {
        // save all installed file path to ${appid}.install
        const auto installRulePath = dir.filePath("../" + QFileInfo(installFilepath).fileName());
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

} // namespace

Builder::Builder(const api::types::v1::BuilderProject &project,
                 const QDir &workingDir,
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

    auto arch = package::Architecture::currentCPUArchitecture();
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
                       .arg("Name", -20) // NOLINT
                       .arg("Type", -15) // NOLINT
                       .arg("Url", -75)  // NOLINT
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
                   .arg("Package", -25) // NOLINT
                   .arg("Version", -15) // NOLINT
                   .arg("Module", -15)  // NOLINT
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
                            .arg(runtime->id, -25)                 // NOLINT
                            .arg(runtime->version.toString(), -15) // NOLINT
                            .arg("develop", -15)                   // NOLINT
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
                        .arg(base->id, -25)                 // NOLINT
                        .arg(base->version.toString(), -15) // NOLINT
                        .arg("develop", -15)                // NOLINT
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

    std::string scriptContent = R"(#!/bin/bash
set -e

# This file is generated by `build` in linglong.yaml
# DO NOT EDIT IT
)";

    scriptContent.append(project.build);
    scriptContent.push_back('\n');
    scriptContent.append("# POST BUILD PROCESS\n");
    scriptContent.append(LINGLONG_BUILDER_HELPER "/main-check.sh\n");
    auto writeBytes = scriptContent.size();
    auto bytesWrite = entry.write(scriptContent.c_str());

    if (bytesWrite == -1 || writeBytes != bytesWrite) {
        return LINGLONG_ERR("Failed to write script content to file", entry);
    }

    entry.close();
    if (entry.error() != QFile::NoError) {
        return LINGLONG_ERR(entry);
    }

    qDebug() << "build script:" << QString::fromStdString(scriptContent);

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
        .runtimeDir = {},
        .baseDir = *baseLayerDir,
        .appDir = {},
        .patches = {},
        .mounts = {},
        .masks = {},
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
    if (QDir(LINGLONG_BUILDER_HELPER).exists()) {
        opts.mounts.push_back({
          .destination = LINGLONG_BUILDER_HELPER,
          .gidMappings = {},
          .options = { { "rbind", "ro" } },
          .source = LINGLONG_BUILDER_HELPER,
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

    auto process = ocppi::runtime::config::types::Process{};
    process.args = arguments;
    process.cwd = "/project";
    process.env = { {
      "PREFIX=" + installPrefix.toStdString(),
      "TRIPLET=" + arch->getTriplet().toStdString(),
    } };
    process.noNewPrivileges = true;
    process.terminal = true;

    printMessage("[Start Build]");
    auto result = (*container)->run(process);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    qDebug() << "run container success";
    if (cfg.skipCommitOutput) {
        qWarning() << "skip commit output";
        return LINGLONG_OK;
    }

    qDebug() << "create binary output";
    QDir binaryOutput = this->workingDir.absoluteFilePath("linglong/output/binary/files");
    if (!binaryOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + binaryOutput.absolutePath() + ": failed.");
    }

    qDebug() << "generate application configure";
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
      QStringList() << "-e" << scriptFile << QString::fromStdString(this->project.package.id)
                    << developOutput.absolutePath());
    if (!output) {
        return LINGLONG_ERR(output);
    }

    printMessage("[Install Files]");
    qDebug() << "split develop";

    const auto installRulePath =
      workingDir.filePath(QString::fromStdString(project.package.id) + ".install");
    auto ret = splitDevelop(installRulePath,
                            developOutput.absolutePath(),
                            binaryOutput.absolutePath(),
                            installPrefix,
                            [](int percentage) {
                                printProgress(percentage);
                            });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }
    printMessage("");

    qDebug() << "generate entries";
    if (this->project.package.kind != "runtime") {
        QDir binaryFiles = this->workingDir.absoluteFilePath("linglong/output/binary/files");
        QDir binaryEntries = this->workingDir.absoluteFilePath("linglong/output/binary/entries");
        QDir developFiles = this->workingDir.absoluteFilePath("linglong/output/develop/files");
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
        if (!QFile::link("../files/share", developEntries.absoluteFilePath("share"))) {
            return LINGLONG_ERR("link entries share to files share: failed");
        }

        if (binaryFiles.exists("lib/systemd/user")) {
            if (!binaryEntries.mkpath("share/systemd/user")) {
                return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
            }
            auto ret = util::copyDir(binaryFiles.filePath("lib/systemd/user"),
                                     binaryEntries.absoluteFilePath("share/systemd/user"));
            if (!ret.has_value()) {
                return LINGLONG_ERR(ret);
            }
        }
        if (developFiles.exists("lib/systemd/user")) {
            if (!developOutput.mkpath("share/systemd/user")) {
                return LINGLONG_ERR("mkpath files/share/systemd/user: failed");
            }
            auto ret = util::copyDir(developFiles.filePath("lib/systemd/user"),
                                     developEntries.absoluteFilePath("share/systemd/user"));
            if (!ret.has_value()) {
                return LINGLONG_ERR(ret);
            }
        }
        if (project.command.value_or(std::vector<std::string>{}).empty()) {
            return LINGLONG_ERR("command field is required, please specify!");
        }
    }

    qDebug() << "generate app info";

    static QRegularExpression appIDReg(
      "[a-zA-Z0-9][-a-zA-Z0-9]{0,62}(\\.[a-zA-Z0-9][-a-zA-Z0-9]{0,62})+");
    auto matches = appIDReg.match(QString::fromStdString(this->project.package.id));
    if (not(matches.isValid() && matches.hasMatch())) {
        qWarning() << "This app id does not follow the reverse domain name notation convention. "
                      "See https://wikipedia.org/wiki/Reverse_domain_name_notation";
    }
    // when the base version is likes 20.0.0.1, warning that it is a full version
    // if the base version is likes 20.0.0, we should also write 20.0.0 to info.json
    if (fuzzyBase->version->tweak) {
        qWarning() << fuzzyBase->toString() << "is set a full version.";
    } else {
        base->version.tweak = std::nullopt;
    }

    auto info = api::types::v1::PackageInfoV2{
        .arch = { package::Architecture::currentCPUArchitecture()->toString().toStdString() },
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

    qDebug() << "copy linglong.yaml to output";
    QFile::copy(this->workingDir.absoluteFilePath("linglong.yaml"),
                this->workingDir.absoluteFilePath("linglong/output/binary/linglong.yaml"));
    QFile::copy(this->workingDir.absoluteFilePath("linglong.yaml"),
                this->workingDir.absoluteFilePath("linglong/output/develop/linglong.yaml"));

    printMessage("[Commit Contents]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", -25) // NOLINT
                   .arg("Version", -15) // NOLINT
                   .arg("Module", -15)  // NOLINT
                   .arg("Status")
                   .toStdString(),
                 2);
    printReplacedText(QString("%1%2%3%4")
                        .arg(info.id.c_str(), -25)      // NOLINT
                        .arg(info.version.c_str(), -15) // NOLINT
                        .arg("binary", -15)             // NOLINT
                        .arg("committing")
                        .toStdString(),
                      2);
    qDebug() << "import binary to layers";
    package::LayerDir binaryOutputLayerDir = binaryOutput.absoluteFilePath("..");
    result = this->repo.remove(*ref);
    if (!result) {
        qWarning() << "remove" << ref->toString() << result.error().message();
    }
    auto localLayer = this->repo.importLayerDir(binaryOutputLayerDir);
    if (!localLayer) {
        return LINGLONG_ERR(localLayer);
    }
    printReplacedText(QString("%1%2%3%4")
                        .arg(info.id.c_str(), -25)      // NOLINT
                        .arg(info.version.c_str(), -15) // NOLINT
                        .arg("binary", -15)             // NOLINT
                        .arg("complete\n")
                        .toStdString(),
                      2);

    printReplacedText(QString("%1%2%3%4")
                        .arg(info.id.c_str(), -25)      // NOLINT
                        .arg(info.version.c_str(), -15) // NOLINT
                        .arg("develop", -15)            // NOLINT
                        .arg("committing")
                        .toStdString(),
                      2);
    qDebug() << "import develop to layers";
    package::LayerDir developOutputLayerDir = developOutput.absoluteFilePath("..");
    result = this->repo.remove(*ref, true);
    if (!result) {
        qWarning() << "remove" << ref->toString() << result.error().message();
    }
    localLayer = this->repo.importLayerDir(developOutputLayerDir);
    if (!localLayer) {
        return LINGLONG_ERR(localLayer);
    }
    printReplacedText(QString("%1%2%3%4")
                        .arg(info.id.c_str(), -25)      // NOLINT
                        .arg(info.version.c_str(), -15) // NOLINT
                        .arg("develop", -15)            // NOLINT
                        .arg("complete\n")
                        .toStdString(),
                      2);
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

    if (project.exclude) {
        packager.exclude(project.exclude.value());
    }

    if (project.include) {
        packager.include(project.include.value());
    }

    auto baseRef = pullDependency(QString::fromStdString(this->project.base),
                                  this->repo,
                                  false,
                                  cfg.skipPullDepend.has_value() && *cfg.skipPullDepend);
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }
    auto baseDir = this->repo.getLayerDir(*baseRef);
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
        auto runtimeDir = this->repo.getLayerDir(*ref);
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

    auto uabFile = QString{ "%1_%2_%3_%4.uab" }.arg(curRef->id,
                                                    curRef->arch.toString(),
                                                    curRef->version.toString(),
                                                    curRef->channel);
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
                                                   const QString &repoUrl,
                                                   const QString &repoName)
{
    LINGLONG_TRACE("push reference to remote repository");

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto cfg = this->repo.getConfig();
    auto oldCfg = cfg;

    auto _ = // NOLINT
      utils::finally::finally([this, &oldCfg]() {
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
              std::filesystem::path{ curDir->absolutePath().toStdString() };
            std::for_each(innerBinds->cbegin(), innerBinds->cend(), bindInnerMount);
        }
    }

    options.mounts = std::move(applicationMounts);

    auto container = this->containerBuilder.create(options);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process process;

    if (!args.isEmpty()) {
        process.args = std::vector<std::string>{};
        for (const auto &arg : args) {
            process.args->push_back(arg.toStdString());
        }
    } else {
        process.args = this->project.command;
        if (!process.args) {
            process.args = std::vector<std::string>{ "bash" };
        }
    }

    auto result = container->data()->run(process);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
