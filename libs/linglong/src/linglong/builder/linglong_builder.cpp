/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/printer.h"
#include "linglong/package/architecture.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/reference.h"
#include "linglong/package/uab_packager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/packageinfo_handler.h"
#include "ocppi/runtime/RunOption.hpp"
#include "source_fetcher.h"

#include <nlohmann/json.hpp>
#include <qdebug.h>
#include <sys/prctl.h>
#include <yaml-cpp/yaml.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThread>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

namespace linglong::builder {

namespace {

// generate a unique id for then container
QString genContainerID(const package::Reference &ref)
{
    auto content = ref.id + "-";
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    content.append(QString::fromStdString(std::to_string(now)));

    // 如果LINGLONG_DEBUG为true，则对ID进行编码，避免外部依赖该ID规则
    // 调试模式则不进行二次编码，便于跟踪排查
    if (::getenv("LINGLONG_DEBUG") != nullptr) {
        return content;
    }

    return QCryptographicHash::hash(content.toUtf8(), QCryptographicHash::Sha256).toHex();
}

/*!
 * 拷贝目录
 * @param src 来源
 * @param dst 目标
 * @return
 */
utils::error::Result<void> inline copyDir(const QString &src, const QString &dst)
{
    LINGLONG_TRACE(QString("copy %1 to %2").arg(src).arg(dst));

    QDir srcDir(src);
    QDir dstDir(dst);

    if (!dstDir.exists()) {
        if (!dstDir.mkpath(".")) {
            return LINGLONG_ERR("create " + dstDir.absolutePath() + ": failed");
        };
    }

    QFileInfoList list = srcDir.entryInfoList();

    for (const auto &info : list) {
        if (info.fileName() == "." || info.fileName() == "..") {
            continue;
        }

        if (info.isDir()) {
            // 穿件文件夹，递归调用
            auto ret = copyDir(info.filePath(), dst + "/" + info.fileName());
            if (!ret.has_value()) {
                return ret;
            }
            continue;
        }

        if (!info.isSymLink()) {
            // 拷贝文件
            QFile file(info.filePath());
            if (!file.copy(dst + "/" + info.fileName())) {
                return LINGLONG_ERR(file);
            };
            continue;
        }

        std::array<char, PATH_MAX + 1> buf{};
        auto size = readlink(info.filePath().toStdString().c_str(), buf.data(), PATH_MAX);
        if (size == -1) {
            return LINGLONG_ERR("readlink failed! " + info.filePath());
        }

        QFileInfo originFile(info.symLinkTarget());
        QString newLinkFile = dst + "/" + info.fileName();

        if (buf.at(0) == '/') {
            if (!QFile::link(info.symLinkTarget(), newLinkFile)) {
                return LINGLONG_ERR("Failed to create link: " + QFile().errorString());
            };
            continue;
        }

        // caculator the relative path
        QDir linkFileDir(info.dir());
        QString relativePath = linkFileDir.relativeFilePath(originFile.path());
        auto newOriginFile = relativePath.endsWith("/")
          ? relativePath + originFile.fileName()
          : relativePath + "/" + originFile.fileName();
        if (!QFile::link(newOriginFile, newLinkFile)) {
            return LINGLONG_ERR("Failed to create link: " + QFile().errorString());
        };
    }
    return LINGLONG_OK;
}

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
        if (!sources.at(pos).url.has_value()) {
            return LINGLONG_ERR("source missing url");
        }
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
                                                        const std::string &module,
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
    auto baseLayerDir = repo.getLayerDir(*ref, module);
    if (baseLayerDir) {
        return *ref;
    }

    auto tmpTask = service::PackageTask::createTemporaryTask();
    auto partChanged = [&ref, module](const uint fetched, const uint requested) {
        auto percentage = (uint)((((double)fetched) / requested) * 100);
        auto progress = QString("(%1/%2 %3%)").arg(fetched).arg(requested).arg(percentage);
        printReplacedText(QString("%1%2%3%4 %5")
                            .arg(ref->id, -25)                        // NOLINT
                            .arg(ref->version.toString(), -15)        // NOLINT
                            .arg(QString::fromStdString(module), -15) // NOLINT
                            .arg("downloading")
                            .arg(progress)
                            .toStdString(),
                          2);
    };
    QObject::connect(&tmpTask, &service::PackageTask::PartChanged, partChanged);
    printReplacedText(QString("%1%2%3%4 %5")
                        .arg(ref->id, -25)                        // NOLINT
                        .arg(ref->version.toString(), -15)        // NOLINT
                        .arg(QString::fromStdString(module), -15) // NOLINT
                        .arg("waiting")
                        .arg("...")
                        .toStdString(),
                      2);
    repo.pull(tmpTask, *ref, module);
    if (tmpTask.state() == linglong::api::types::v1::State::Failed) {
        return LINGLONG_ERR("pull " + ref->toString() + " failed", std::move(tmpTask).takeError());
    }
    return *ref;
}

// 安装模块文件
utils::error::Result<void> installModule(QStringList installRules,
                                         const QDir &buildOutput,
                                         const QDir &moduleOutput,
                                         const std::function<void(int)> &handleProgress)
{
    LINGLONG_TRACE("install module file");
    buildOutput.mkpath(".");
    moduleOutput.mkpath(".");
    const QString src = buildOutput.absolutePath();
    const QString dest = moduleOutput.absolutePath();

    // 复制目录、文件和超链接
    auto installFile = [&](const QFileInfo &info,
                           const QString &dstPath) -> utils::error::Result<void> {
        LINGLONG_TRACE("install file");
        if (info.isDir()) {
            QDir().mkpath(dstPath);
            return LINGLONG_OK;
        } else {
            QDir(dstPath.left(dstPath.lastIndexOf('/'))).mkpath(".");
            // QDir(dstPath).mkpath("..");
            QFile::rename(info.filePath(), dstPath);
            return LINGLONG_OK;
        }
    };

    auto ruleIndex = 0;
    for (auto rule : installRules) {
        rule = rule.simplified();
        // 跳过注释
        if (rule.isEmpty() || rule.startsWith("#")) {
            continue;
        }
        qDebug() << "install rule" << rule;
        // 计算进度
        ruleIndex++;
        auto percentage = ruleIndex * 100 / installRules.length(); // NOLINT
        // 统计进度
        if (handleProgress) {
            handleProgress(percentage);
        }
        // 如果不以^符号开头，当作普通路径使用
        if (!rule.startsWith("^")) {
            // append $PROJECT_ROOT/output/_build/files to prefix
            rule = src + "/" + rule;
            QFileInfo info(rule);
            // 链接指向的文件如果不存在，info.exists会返回false
            // 所以要先判断文件是否是链接
            if (info.isSymLink() || info.exists()) {
                const QString dstPath = info.absoluteFilePath().replace(src, dest);
                auto ret = installFile(info, dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            } else {
                qWarning() << "missing file" << rule;
            }
            continue;
        }
        if (rule.startsWith("^/")) {
            rule = "^" + src + rule.mid(1);
        } else {
            rule = "^" + src + "/" + rule.mid(1);
        }
        QRegularExpression regexp(rule);
        // reverse files in src
        QDirIterator iter(src,
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            iter.next();
            if (regexp.match(iter.fileInfo().absoluteFilePath()).hasMatch()) {
                const QString dstPath = iter.fileInfo().absoluteFilePath().replace(src, dest);
                qDebug() << rule << "match" << iter.filePath();
                auto ret = installFile(iter.fileInfo(), dstPath);
                if (!ret.has_value()) {
                    return LINGLONG_ERR(ret);
                }
            }
        }
    }
    handleProgress(100);
    return LINGLONG_OK;
}

} // namespace

utils::error::Result<void> cmdListApp(repo::OSTreeRepo &repo)
{
    LINGLONG_TRACE("cmd list app");
    auto list = repo.listLocal();
    if (!list.has_value()) {
        return LINGLONG_ERR("list local pkg", list);
    }
    std::vector<std::string> refs;
    for (const auto &item : *list) {
        auto ref = package::Reference::fromPackageInfo(item);
        if (!ref.has_value()) {
            continue;
        }
        refs.push_back(ref->toString().toStdString());
    }
    std::sort(refs.begin(), refs.end());
    auto it = std::unique(refs.begin(), refs.end());
    refs.erase(it, refs.end());
    for (const auto &ref : refs) {
        std::cout << ref << std::endl;
    }
    return LINGLONG_OK;
}

utils::error::Result<void> cmdRemoveApp(repo::OSTreeRepo &repo, std::vector<std::string> refs)
{
    LINGLONG_TRACE("cmd remove app");
    for (const auto &ref : refs) {
        auto r = package::Reference::parse(QString::fromStdString(ref));
        if (!r.has_value()) {
            std::cerr << ref << ": " << r.error().message().toStdString() << std::endl;
            continue;
        }
        auto modules = repo.getModuleList(*r);
        for (const auto &module : modules) {
            auto v = repo.remove(*r, module);
            if (!v.has_value()) {
                std::cerr << ref << ": " << v.error().message().toStdString() << std::endl;
                continue;
            }
        }
    }
    auto v = repo.prune();
    if (!v.has_value()) {
        std::cerr << v.error().message().toStdString();
    }
    v = repo.mergeModules();
    if (!v.has_value()) {
        std::cerr << v.error().message().toStdString();
    }
    return LINGLONG_OK;
}

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
    /*** Fetch Source Stage ***/
    if (this->project.sources && !this->buildOptions.skipFetchSource) {
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
    /*** Pull Depend Stage ***/
    printMessage("[Processing Dependency]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", -25) // NOLINT
                   .arg("Version", -15) // NOLINT
                   .arg("Module", -15)  // NOLINT
                   .arg("Status")
                   .toStdString(),
                 2);
    auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(this->project.base));
    if (!fuzzyBase) {
        return LINGLONG_ERR(fuzzyBase);
    }
    auto baseRef =
      pullDependency(*fuzzyBase, this->repo, "binary", this->buildOptions.skipPullDepend);
    if (!baseRef) {
        return LINGLONG_ERR("pull base binary", baseRef);
    }
    printReplacedText(QString("%1%2%3%4")
                        .arg(baseRef->id, -25)                 // NOLINT
                        .arg(baseRef->version.toString(), -15) // NOLINT
                        .arg("binary", -15)                    // NOLINT
                        .arg("complete\n")
                        .toStdString(),
                      2);
    baseRef = pullDependency(*fuzzyBase, this->repo, "develop", this->buildOptions.skipPullDepend);
    if (!baseRef) {
        return LINGLONG_ERR("pull base develop", baseRef);
    }
    auto ret = this->repo.mergeModules();
    if (!ret.has_value()) {
        return LINGLONG_ERR(ret);
    }
    printReplacedText(QString("%1%2%3%4")
                        .arg(baseRef->id, -25)                 // NOLINT
                        .arg(baseRef->version.toString(), -15) // NOLINT
                        .arg("develop", -15)                   // NOLINT
                        .arg("complete\n")
                        .toStdString(),
                      2);
    auto baseLayerDir = this->repo.getMergedModuleDir(*baseRef, false);
    if (!baseLayerDir) {
        baseLayerDir = this->repo.getLayerDir(*baseRef, "develop");
        if (!baseLayerDir.has_value()) {
            return LINGLONG_ERR("get base layer dir", baseLayerDir);
        }
        return LINGLONG_ERR("get base layer dir", baseLayerDir);
    }

    std::optional<package::FuzzyReference> fuzzyRuntime;
    std::optional<package::Reference> runtimeRef;
    std::optional<package::LayerDir> runtimeLayerDir;
    if (this->project.runtime) {
        auto fuzzyRuntimeRet =
          package::FuzzyReference::parse(QString::fromStdString(*this->project.runtime));
        if (!fuzzyRuntimeRet) {
            return LINGLONG_ERR(fuzzyRuntimeRet);
        }
        fuzzyRuntime = *fuzzyRuntimeRet;
        auto runtimeRet =
          pullDependency(*fuzzyRuntime, this->repo, "binary", this->buildOptions.skipPullDepend);
        if (!runtimeRet) {
            return LINGLONG_ERR("pull runtime", runtimeRet);
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg(runtimeRet->id, -25)                 // NOLINT
                            .arg(runtimeRet->version.toString(), -15) // NOLINT
                            .arg("binary", -15)                       // NOLINT
                            .arg("complete\n")
                            .toStdString(),
                          2);
        runtimeRet =
          pullDependency(*fuzzyRuntime, this->repo, "develop", this->buildOptions.skipPullDepend);
        if (!runtimeRet) {
            return LINGLONG_ERR("pull runtime", runtimeRet);
        }
        auto ret = this->repo.mergeModules();
        if (!ret.has_value()) {
            return LINGLONG_ERR(ret);
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg(runtimeRet->id, -25)                 // NOLINT
                            .arg(runtimeRet->version.toString(), -15) // NOLINT
                            .arg("develop", -15)                      // NOLINT
                            .arg("complete\n")
                            .toStdString(),
                          2);
        auto runtimeLayerDirRet = this->repo.getMergedModuleDir(*runtimeRet, false);
        if (!runtimeLayerDirRet.has_value()) {
            runtimeLayerDirRet = this->repo.getLayerDir(*runtimeRet, "develop");
            if (!runtimeLayerDirRet.has_value()) {
                return LINGLONG_ERR("get runtime layer dir", runtimeLayerDirRet);
            }
        }
        runtimeRef = *runtimeRet;
        runtimeLayerDir = *runtimeLayerDirRet;
    }

    /*** Run Container Stage ***/
    if (this->buildOptions.skipRunContainer) {
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
    if (!this->buildOptions.skipStripSymbols) {
        scriptContent.push_back('\n');
        scriptContent.append("# enable strip symbols\n");
        scriptContent.append("export CFLAGS=\"-g $CFLAGS\"\n");
        scriptContent.append("export CXXFLAGS=\"-g $CFLAGS\"\n");
    }
    scriptContent.append(project.build);
    scriptContent.push_back('\n');
    // Do some checks after run container
    if (!this->buildOptions.skipCheckOutput && this->project.package.kind == "app") {
        scriptContent.append("# POST BUILD PROCESS\n");
        scriptContent.append(LINGLONG_BUILDER_HELPER "/main-check.sh\n");
    }
    if (!this->buildOptions.skipStripSymbols) {
        scriptContent.append(LINGLONG_BUILDER_HELPER "/symbols-strip.sh\n");
    }

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

    QDir buildOutput = this->workingDir.absoluteFilePath("linglong/output/_build");
    if (!buildOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + buildOutput.path() + ": failed.");
    }
    qDebug() << "create develop output success";

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }
    auto opts = runtime::ContainerOptions{
        .appID = QString::fromStdString(this->project.package.id),
        .containerID = genContainerID(*ref),
        .runtimeDir = {},
        .baseDir = *baseLayerDir,
        .appDir = {},
        .patches = {},
        .mounts = {},
        .hooks = {},
        .masks = {},
    };
    if (runtimeLayerDir.has_value()) {
        opts.runtimeDir = *runtimeLayerDir;
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
      .source = buildOutput.path().toStdString(),
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

    // initialize the cache dir
    QDir appCache = this->workingDir.absoluteFilePath("linglong/cache");
    QDir appFontCache = appCache.absoluteFilePath("fontconfig");
    if (!appFontCache.mkpath(".")) {
        return LINGLONG_ERR("make path " + appFontCache.absolutePath() + ": failed.");
    }
    QDir appFonts = appCache.absoluteFilePath("fonts");
    if (!appFonts.mkpath(".")) {
        return LINGLONG_ERR("make path " + appFonts.absolutePath() + ": failed.");
    }
    // write ld.so.conf
    QFile ldsoconf = appCache.absoluteFilePath("ld.so.conf");
    if (!ldsoconf.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(ldsoconf);
    }
    QString ldRawConf = R"(/runtime/lib
/runtime/lib/@triplet@
include /runtime/etc/ld.so.conf
/opt/apps/@id@/files/lib
/opt/apps/@id@/files/lib/@triplet@
include /opt/apps/@id@/files/etc/ld.so.conf)";
    ldRawConf.replace("@id@", QString::fromStdString(this->project.package.id));
    ldRawConf.replace("@triplet@", arch->getTriplet());
    ldsoconf.write(ldRawConf.toUtf8());
    // must be closed here, this conf will be used later.
    ldsoconf.close();
    // write fonts.conf
    QFile fontsConf = appFonts.absoluteFilePath("fonts.conf");
    if (!fontsConf.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(fontsConf);
    }
    QString fontsRawConf = R"(<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>/run/linglong/fonts</dir>
  <include ignore_missing="yes">/opt/apps/@id@/files/etc/fonts/fonts.conf</include>
</fontconfig>)";
    fontsRawConf.replace("@id@", QString::fromStdString(this->project.package.id));
    fontsConf.write(fontsRawConf.toUtf8());

    opts.mounts.push_back({
      .destination = "/run/linglong/cache",
      .gidMappings = {},
      .options = { { "rbind", "rw" } },
      .source = appCache.absolutePath().toStdString(),
      .type = "bind",
      .uidMappings = {},
    });
    // it seems that generating font cache during build is unnecessary
    std::vector<ocppi::runtime::config::types::Hook> generateCache{};
    std::vector<std::string> ldconfigCmd = { "/sbin/ldconfig",
                                             "-C",
                                             "/run/linglong/cache/ld.so.cache" };
    generateCache.push_back(ocppi::runtime::config::types::Hook{
      .args = std::move(ldconfigCmd),
      .env = {},
      .path = "/sbin/ldconfig",
      .timeout = {},
    });
    opts.hooks.startContainer = std::move(generateCache);

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
    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    qDebug() << "run container success";

    /*** Commit Output Stage ***/
    if (this->buildOptions.skipCommitOutput) {
        qWarning() << "skip commit output";
        return LINGLONG_OK;
    }

    qDebug() << "generate application configure";
    // generate application's configure file
    auto scriptFile = QString(LINGLONG_LIBEXEC_DIR) + "/app-conf-generator";
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        qDebug() << "Dumping app-conf-generator from qrc...";
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath("app-conf-generator");
        QFile::copy(":/scripts/app-conf-generator", scriptFile);
    }
    auto output = utils::command::Exec(
      "bash",
      QStringList() << "-e" << scriptFile << QString::fromStdString(this->project.package.id)
                    << buildOutput.path());

    auto appIDPrintWidth = -this->project.package.id.size() + -5;
    printMessage("[Install Files]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", appIDPrintWidth)
                   .arg("Version", -15)
                   .arg("Module", -15)
                   .arg("Status")
                   .toStdString(),
                 2);
    // 保存全量的develop, runtime需要对旧的ll-builder保持兼容
    if (this->buildOptions.fullDevelop) {
        QDir moduleDir = this->workingDir.absoluteFilePath("linglong/output/develop/files");
        auto ret = copyDir(buildOutput.path(), moduleDir.path());
        if (!ret.has_value()) {
            return ret;
        }
    }
    std::vector<api::types::v1::BuilderProjectModules> projectModules;

    auto hasDevelop = false;
    if (this->project.modules.has_value()) {
        for (const auto &module : *this->project.modules) {
            if (module.name == "develop") {
                hasDevelop = true;
            }
            projectModules.push_back(module);
        }
    }
    // 如果没有develop模块，则添加一个默认的，包含include/**, lib/debug/**, lib/**.a三种匹配
    if (!this->buildOptions.fullDevelop && !hasDevelop) {
        projectModules.push_back(api::types::v1::BuilderProjectModules{
          .files = { "^/include/.+", "^/lib/debug/.+", "^/lib/.+\\.a$" },
          .name = "develop",
        });
    }

    for (const auto &module : projectModules) {
        auto name = QString::fromStdString(module.name);
        printReplacedText(QString("%1%2%3%4")
                            .arg(this->project.package.id.c_str(), appIDPrintWidth)
                            .arg(this->project.package.version.c_str(), -15)
                            .arg(name, -15)
                            .arg("installing")
                            .toStdString(),
                          2);
        qDebug() << "install modules" << name;
        QDir moduleDir = this->workingDir.absoluteFilePath("linglong/output/" + name);
        QStringList installRules;
        for (const auto &file : module.files) {
            installRules.append(file.c_str());
        }
        installRules.removeDuplicates();
        auto ret = installModule(installRules,
                                 buildOutput.path(),
                                 moduleDir.filePath("files"),
                                 [](int percentage) {});
        if (!ret.has_value()) {
            return LINGLONG_ERR("install module", ret);
        }
        // save install fule to ${appid}.install
        auto appID = QString::fromStdString(project.package.id);
        const auto installRulePath = moduleDir.filePath(appID + ".install");
        QFile configFile(installRulePath);
        if (!configFile.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR("open file " + installRulePath, configFile);
        }
        if (configFile.write(installRules.join('\n').toUtf8()) < 0) {
            return LINGLONG_ERR("write file " + installRulePath, configFile);
        }
        configFile.close();
        printReplacedText(QString("%1%2%3%4")
                            .arg(this->project.package.id.c_str(), appIDPrintWidth)
                            .arg(this->project.package.version.c_str(), -15)
                            .arg(name, -15)
                            .arg("complete\n")
                            .toStdString(),
                          2);
    }
    // save binary install files
    {
        QStringList installRules;
        auto installFilepath = this->workingDir.absoluteFilePath(
          QString::fromStdString(this->project.package.id) + ".install");
        // TODO 兼容$appid.install文件，后续版本删除
        if (QFile::exists(installFilepath)) {
            qWarning() << "$appid.install is deprecated. see "
                          "https://linglong.space/guide/ll-builder/modules.html";
            QFile configFile(installFilepath);
            if (!configFile.open(QIODevice::ReadOnly)) {
                return LINGLONG_ERR("open file", configFile);
            }
            for (auto &line : QString(configFile.readAll()).split('\n')) {
                installRules.append(line.replace(installPrefix, ""));
            }
            // remove empty or duplicate lines
            installRules.removeAll("");
            installRules.removeDuplicates();
        } else {
            QDirIterator iter(buildOutput.path(),
                              QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                              QDirIterator::Subdirectories);
            while (iter.hasNext()) {
                iter.next();
                auto filepath = iter.filePath().mid(buildOutput.path().length());
                installRules.append(filepath);
            }
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg(this->project.package.id.c_str(), appIDPrintWidth)
                            .arg(this->project.package.version.c_str(), -15)
                            .arg("binary", -15)
                            .arg("installing")
                            .toStdString(),
                          2);
        QDir moduleDir = this->workingDir.absoluteFilePath("linglong/output/binary");
        auto ret = installModule(installRules,
                                 buildOutput.path(),
                                 moduleDir.filePath("files"),
                                 [](int percentage) {});
        if (!ret.has_value()) {
            return LINGLONG_ERR("install module", ret);
        }
        // save install fule to ${appid}.install
        auto appID = QString::fromStdString(project.package.id);
        const auto installRulePath = moduleDir.filePath(appID + ".install");
        QFile configFile(installRulePath);
        if (!configFile.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR("open file " + installRulePath, configFile);
        }
        if (configFile.write(installRules.join('\n').toUtf8()) < 0) {
            return LINGLONG_ERR("write file " + installRulePath, configFile);
        }
        configFile.close();
        printReplacedText(QString("%1%2%3%4")
                            .arg(this->project.package.id.c_str(), appIDPrintWidth)
                            .arg(this->project.package.version.c_str(), -15)
                            .arg("binary", -15)
                            .arg("complete\n")
                            .toStdString(),
                          2);
    }

    printMessage("");

    qDebug() << "generate entries";
    if (this->project.package.kind != "runtime") {
        // 仅导出名单中的目录，以避免意外文件影响系统功能
        const QStringList exportPaths = {
            "share/applications", // Copy desktop files
            "share/mime",         // Copy MIME Type files
            "share/icons",        // Icons
            "share/dbus-1",       // D-Bus service files
            "share/gnome-shell",  // Search providers
            "share/appdata",      // Copy appdata/metainfo files (legacy path)
            "share/metainfo",     // Copy appdata/metainfo files
            "share/plugins",      // Copy plugins conf，The configuration files provided by some
                                  // applications maybe used by the host dde-file-manager.
            "share/systemd",      // copy systemd service files
            "share/deepin-manual" // copy deepin-manual files
        };

        QDir binaryFiles = this->workingDir.absoluteFilePath("linglong/output/binary/files");
        QDir binaryEntries = this->workingDir.absoluteFilePath("linglong/output/binary/entries");

        if (!binaryEntries.mkpath(".")) {
            return LINGLONG_ERR("make path " + binaryEntries.absolutePath() + ": failed.");
        }

        if (binaryFiles.exists("share")) {
            if (!binaryEntries.mkpath("share")) {
                return LINGLONG_ERR("mkpath files/share: failed");
            }
        }

        for (const auto &path : exportPaths) {
            if (!binaryFiles.exists(path)) {
                continue;
            }
            // appdata是旧版本的metainfo
            if (path == "share/appdata") {
                auto ret = copyDir(binaryFiles.absoluteFilePath(path),
                                   binaryEntries.filePath("share/metainfo"));
                if (!ret.has_value()) {
                    qWarning() << "link binary entries share to files share/" << path << "failed";
                }

                continue;
            }
            auto ret =
              copyDir(binaryFiles.absoluteFilePath(path), binaryEntries.absoluteFilePath(path));
            if (!ret.has_value()) {
                qWarning() << "link binary entries " << path << "to files share: failed";
                continue;
            }
        }

        if (binaryFiles.exists("lib/systemd/user")) {
            // 配置放到share/systemd/user或lib/systemd/user对systemd来说基本等价
            // 但玲珑仅将share导出到XDG_DATA_DIR，所以要将lib/systemd/user的内容复制到share/systemd/user
            if (!binaryEntries.mkpath("share/systemd/user")) {
                qWarning() << "mkpath files/share/systemd/user: failed";
            }
            auto ret = copyDir(binaryFiles.filePath("lib/systemd/user"),
                               binaryEntries.absoluteFilePath("share/systemd/user"));
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

    auto info = api::types::v1::PackageInfoV2{
        .arch = { package::Architecture::currentCPUArchitecture()->toString().toStdString() },
        .channel = ref->channel.toStdString(),
        .command = project.command,
        .description = this->project.package.description,
        .id = this->project.package.id,
        .kind = this->project.package.kind,
        .name = this->project.package.name,
        .permissions = this->project.permissions,
        .runtime = {},
        .schemaVersion = PACKAGE_INFO_VERSION,
        .version = this->project.package.version,
    };
    // when the base version is likes 20.0.0.1, warning that it is a full version
    // if the base version is likes 20.0.0, we should also write 20.0.0 to info.json
    if (fuzzyBase->version->tweak) {
        qWarning() << fuzzyBase->toString() << "is set a full version.";
    } else {
        baseRef->version.tweak = std::nullopt;
    }
    info.base = baseRef->toString().toStdString();
    if (runtimeRef) {
        // the runtime version is same as base.
        if (fuzzyRuntime->version->tweak) {
            qWarning() << fuzzyRuntime->toString() << "is set a full version.";
        } else {
            runtimeRef->version.tweak = std::nullopt;
        }
        info.runtime = runtimeRef->toString().toStdString();
    }
    // 从本地仓库清理旧的ref
    auto existsModules = this->repo.getModuleList(*ref);
    for (const auto &module : existsModules) {
        result = this->repo.remove(*ref, module);
        if (!result) {
            qDebug() << "remove" << ref->toString() << result.error().message();
        }
    }
    // 推送新的ref到本地仓库
    QStringList modules = { "binary" };
    if (this->buildOptions.fullDevelop) {
        modules.push_back("develop");
    }
    for (const auto &module : projectModules) {
        modules.push_back(module.name.c_str());
    }
    printMessage("[Commit Contents]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", appIDPrintWidth) // NOLINT
                   .arg("Version", -15)             // NOLINT
                   .arg("Module", -15)              // NOLINT
                   .arg("Status")
                   .toStdString(),
                 2);
    for (const auto &module : modules) {
        QDir moduleOutput = this->workingDir.absoluteFilePath("linglong/output/" + module);
        info.packageInfoV2Module = module.toStdString();
        auto ret =
          linglong::utils::calculateDirectorySize(moduleOutput.absolutePath().toStdString());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        info.size = static_cast<int>(*ret);

        QFile infoFile = moduleOutput.filePath("info.json");
        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return LINGLONG_ERR(infoFile);
        }

        infoFile.write(nlohmann::json(info).dump().c_str());
        if (infoFile.error() != QFile::NoError) {
            return LINGLONG_ERR(infoFile);
        }
        infoFile.close();

        qDebug() << "copy linglong.yaml to output";

        std::error_code ec;
        std::filesystem::copy(this->projectYamlFile,
                              moduleOutput.filePath("linglong.yaml").toStdString(),
                              ec);
        if (ec) {
            return LINGLONG_ERR(
              QString("copy linglong.yaml to output failed: %1").arg(ec.message().c_str()));
        }
        qDebug() << "import module to layers";
        printReplacedText(QString("%1%2%3%4")
                            .arg(info.id.c_str(), appIDPrintWidth) // NOLINT
                            .arg(info.version.c_str(), -15)        // NOLINT
                            .arg(module, -15)                      // NOLINT
                            .arg("committing")
                            .toStdString(),
                          2);
        auto localLayer = this->repo.importLayerDir(moduleOutput.path());
        if (!localLayer) {
            return LINGLONG_ERR(localLayer);
        }
        printReplacedText(QString("%1%2%3%4")
                            .arg(info.id.c_str(), appIDPrintWidth) // NOLINT
                            .arg(info.version.c_str(), -15)        // NOLINT
                            .arg(module, -15)                      // NOLINT
                            .arg("complete\n")
                            .toStdString(),
                          2);
    }
    auto mergeRet = this->repo.mergeModules();
    if (!mergeRet.has_value()) {
        return mergeRet;
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
        if (auto ret = packager.setIcon(QFileInfo{ option.iconPath }); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (project.exclude) {
        packager.exclude(project.exclude.value());
    }

    if (project.include) {
        packager.include(project.include.value());
    }

    if (option.onlyApp && project.runtime) {
        auto ret = packager.loadBlackList();
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        // load needed libraries
        ret = packager.loadNeededFiles();
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    auto baseFuzzyRef = package::FuzzyReference::parse(QString::fromStdString(this->project.base));
    if (!baseFuzzyRef) {
        return LINGLONG_ERR(baseFuzzyRef);
    }

    auto baseRef =
      this->repo.clearReference(*baseFuzzyRef,
                                linglong::repo::clearReferenceOption{ .fallbackToRemote = false });
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }

    auto baseDir = this->repo.getLayerDir(*baseRef);
    if (!baseDir) {
        return LINGLONG_ERR(baseDir);
    }
    packager.appendLayer(*baseDir);

    if (this->project.runtime) {
        auto runtimeFuzzyRef =
          package::FuzzyReference::parse(QString::fromStdString(this->project.runtime.value()));
        if (!runtimeFuzzyRef) {
            return LINGLONG_ERR(runtimeFuzzyRef);
        }

        auto runtimeRef = this->repo.clearReference(
          *runtimeFuzzyRef,
          linglong::repo::clearReferenceOption{ .fallbackToRemote = false });
        if (!runtimeRef) {
            return LINGLONG_ERR(runtimeRef);
        }

        auto runtimeDir = this->repo.getLayerDir(*runtimeRef);
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
    packager.appendLayer(*appDir); // app layer must be the last of appended layer

    auto uabFile = QString{ "%1_%2_%3_%4.uab" }.arg(curRef->id,
                                                    curRef->arch.toString(),
                                                    curRef->version.toString(),
                                                    curRef->channel);
    if (auto ret = packager.pack(uabFile, option.onlyApp); !ret) {
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

    auto modules = this->repo.getModuleList(*ref);

    package::LayerPackager pkger;
    for (const auto &module : modules) {
        auto layerDir = this->repo.getLayerDir(*ref, module);
        if (!layerDir) {
            qCritical().nospace() << "resolve layer " << ref->toString() << "/" << module.c_str()
                                  << " failed: " << layerDir.error().message();
            continue;
        }

        auto layerFile = QString("%1/%2_%3_%4_%5.layer")
                           .arg(destDir.absolutePath(),
                                ref->id,
                                ref->version.toString(),
                                ref->arch.toString(),
                                module.c_str());
        auto ret = pkger.pack(*layerDir, layerFile);
        if (!ret) {
            qCritical().nospace() << "export layer " << ref->toString() << "/" << module.c_str()
                                  << " failed: " << ret.error().message();
        }
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

linglong::utils::error::Result<void> Builder::push(const std::string &module,
                                                   const std::string &repoUrl,
                                                   const std::string &repoName)
{
    LINGLONG_TRACE("push reference to remote repository");

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    if (repoName.empty() || repoUrl.empty()) {
        return repo.push(*ref, module);
    }

    return repo.pushToRemote(repoName, repoUrl, *ref, module);
}

utils::error::Result<void> Builder::importLayer(repo::OSTreeRepo &ostree, const QString &path)
{
    LINGLONG_TRACE("import layer");
    if (std::filesystem::is_directory(path.toStdString())) {
        auto layerDir = package::LayerDir(path);
        auto info = layerDir.info();
        if (!info) {
            return LINGLONG_ERR(info);
        }
        auto result = ostree.importLayerDir(layerDir);
        if (!result) {
            return LINGLONG_ERR(result);
        }
        return LINGLONG_OK;
    }
    auto layerFile = package::LayerFile::New(path);
    if (!layerFile) {
        return LINGLONG_ERR(layerFile);
    }

    package::LayerPackager pkg;

    auto layerDir = pkg.unpack(*(*layerFile));
    if (!layerDir) {
        return LINGLONG_ERR(layerDir);
    }

    auto result = ostree.importLayerDir(*layerDir);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::run(const QStringList &modules,
                                        const QStringList &args,
                                        const bool &debug)
{
    LINGLONG_TRACE("run application");

    auto curRef = currentReference(this->project);
    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    auto options = runtime::ContainerOptions{
        .appID = curRef->id,
        .containerID = genContainerID(*curRef),
        .runtimeDir = {},
        .baseDir = {},
        .appDir = {},
        .patches = {},
        .mounts = {},
        .hooks = {},
    };

    auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(this->project.base));
    if (!fuzzyBase) {
        return LINGLONG_ERR(fuzzyBase);
    }
    auto baseRef =
      this->repo.clearReference(*fuzzyBase, { .forceRemote = false, .fallbackToRemote = false });
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }
    auto baseDir =
      debug ? this->repo.getMergedModuleDir(*baseRef) : this->repo.getLayerDir(*baseRef, "binary");
    if (!baseDir) {
        return LINGLONG_ERR(baseDir);
    }
    options.baseDir = *baseDir;

    if (this->project.runtime) {
        auto fuzzyRuntime =
          package::FuzzyReference::parse(QString::fromStdString(this->project.runtime.value()));
        if (!fuzzyRuntime) {
            return LINGLONG_ERR(fuzzyRuntime);
        }
        auto runtimeRef =
          this->repo.clearReference(*fuzzyRuntime,
                                    { .forceRemote = false, .fallbackToRemote = false });
        if (!runtimeRef) {
            return LINGLONG_ERR(runtimeRef);
        }
        auto runtimeDir = debug ? this->repo.getMergedModuleDir(*runtimeRef)
                                : this->repo.getLayerDir(*runtimeRef, "binary");
        if (!runtimeDir) {
            return LINGLONG_ERR(runtimeDir);
        }
        options.runtimeDir = *runtimeDir;
    }

    utils::error::Result<package::LayerDir> curDir;
    // mergedDir 会自动在释放时删除临时目录，所以要用变量保留住
    utils::error::Result<std::shared_ptr<package::LayerDir>> mergedDir;
    if (modules.size() > 1) {
        qDebug() << "create temp merge dir." << "ref: " << curRef->toString()
                 << "modules: " << modules;
        mergedDir = this->repo.getMergedModuleDir(*curRef, modules);
        if (!mergedDir.has_value()) {
            return LINGLONG_ERR(mergedDir);
        }
        curDir = *mergedDir->get();
    } else {
        curDir = this->repo.getLayerDir(*curRef);
        if (!curDir) {
            return LINGLONG_ERR(curDir);
        }
    }

    auto info = curDir->info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    if (this->project.package.kind == "app") {
        options.appDir = QDir(curDir->absolutePath());
    } else {
        return LINGLONG_ERR("when kind equals to runtime, it cannot run");
    }

    std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
    if (debug) {
        std::filesystem::path workdir = this->workingDir.absolutePath().toStdString();
        // 生成 host_gdbinit 可使用 gdb --init-command=linglong/host_gdbinit 从宿主机调试
        {
            auto gdbinit = workdir / "linglong/host_gdbinit";
            auto debugDir = workdir / "linglong/output/develop/files/lib/debug";
            std::ofstream hostConf(gdbinit);
            hostConf << "set substitute-path /project " + workdir.string() << std::endl;
            hostConf << "set debug-file-directory " + debugDir.string() << std::endl;
            hostConf << "# target remote :12345";
        }
        // 生成 gdbinit 支持在容器中使用gdb $binary调试
        {
            std::string appPrefix = "/opt/apps/" + this->project.package.id + "/files";
            std::string debugDir = "/usr/lib/debug:/runtime/lib/debug:" + appPrefix + "/lib/debug";
            auto gdbinit = workdir / "linglong/gdbinit";
            std::ofstream f(gdbinit);
            f << "set debug-file-directory " + debugDir << std::endl;

            auto *homeEnv = ::getenv("HOME");
            auto hostHomeDir = std::filesystem::path(homeEnv);
            applicationMounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = hostHomeDir / ".gdbinit",
              .options = { { "ro", "rbind" } },
              .source = gdbinit,
              .type = "bind",
            });
        }
        // 挂载项目目录，便于gdb查看源码
        applicationMounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/project",
          .options = { { "rbind", "ro" } },
          .source = this->workingDir.absolutePath().toStdString(),
          .type = "bind",
        });
    }
    // mount app cache
    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/cache",
      .options = { { "rbind", "rw" } },
      .source = this->workingDir.absoluteFilePath("linglong/cache").toStdString(),
      .type = "bind",
    });
    // mount font cache
    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/var/cache/fontconfig",
      .options = { { "rbind", "rw" } },
      .source = this->workingDir.absoluteFilePath("linglong/cache/fontconfig").toStdString(),
      .type = "bind",
    });

    std::vector<ocppi::runtime::config::types::Hook> generateCache{};
    std::vector<std::string> ldconfigCmd = { "/sbin/ldconfig",
                                             "-C",
                                             "/run/linglong/cache/ld.so.cache" };
    std::vector<std::string> fontconfiCmd = { "fc-cache", "-f" };
    generateCache.push_back(ocppi::runtime::config::types::Hook{
      .args = std::move(ldconfigCmd),
      .env = {},
      .path = "/sbin/ldconfig",
      .timeout = {},
    });
    generateCache.push_back(ocppi::runtime::config::types::Hook{
      .args = std::move(fontconfiCmd),
      .env = {},
      .path = "/bin/fc-cache",
      .timeout = {},
    });
    options.hooks.startContainer = std::move(generateCache);
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

    ocppi::runtime::RunOption opt{};
    auto result = container->data()->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
