/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "linglong/api/types/v1/ExportDirs.hpp"
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
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/RunOption.hpp"
#include "source_fetcher.h"

#include <nlohmann/json.hpp>
#include <qdebug.h>
#include <sys/prctl.h>
#include <sys/sysmacros.h>
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

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace linglong::builder {

namespace {

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

    QFileInfoList list =
      srcDir.entryInfoList(QDir::System | QDir::AllEntries | QDir::NoDotAndDotDot);

    for (const auto &info : list) {
        if (info.isDir() && !info.isSymLink()) {
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
    std::string channel = "main";
    if (project.package.channel.has_value()) {
        channel = *project.package.channel;
    }
    auto ref = package::Reference::create(QString::fromStdString(channel),
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

utils::error::Result<void> pullDependency(const package::Reference &ref,
                                          repo::OSTreeRepo &repo,
                                          const std::string &module) noexcept
{
    LINGLONG_TRACE("pull " + ref.toString());

    // 如果依赖已存在，则直接使用
    if (repo.getLayerDir(ref, module)) {
        return LINGLONG_OK;
    }

    auto tmpTask = service::PackageTask::createTemporaryTask();
    auto partChanged = [&ref, module](const uint fetched, const uint requested) {
        auto percentage = (uint)((((double)fetched) / requested) * 100);
        auto progress = QString("(%1/%2 %3%)").arg(fetched).arg(requested).arg(percentage);
        printReplacedText(QString("%1%2%3%4 %5")
                            .arg(ref.id, -35)                         // NOLINT
                            .arg(ref.version.toString(), -15)         // NOLINT
                            .arg(QString::fromStdString(module), -15) // NOLINT
                            .arg("downloading")
                            .arg(progress)
                            .toStdString(),
                          2);
    };
    QObject::connect(&tmpTask, &service::PackageTask::PartChanged, partChanged);
    printReplacedText(QString("%1%2%3%4 %5")
                        .arg(ref.id, -35)                         // NOLINT
                        .arg(ref.version.toString(), -15)         // NOLINT
                        .arg(QString::fromStdString(module), -15) // NOLINT
                        .arg("waiting")
                        .arg("...")
                        .toStdString(),
                      2);
    repo.pull(tmpTask, ref, module);
    if (tmpTask.state() == linglong::api::types::v1::State::Failed) {
        return LINGLONG_ERR("pull " + ref.toString() + " failed", std::move(tmpTask).takeError());
    }

    return LINGLONG_OK;
}

// 安装模块文件
utils::error::Result<void> installModule(QStringList installRules,
                                         const QDir &buildOutput,
                                         const QDir &moduleOutput)
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
            if (!info.isSymLink()) {
                QDir().mkpath(dstPath);
                return LINGLONG_OK;
            }
            std::error_code ec;
            auto target = std::filesystem::read_symlink(info.filePath().toStdString());
            std::filesystem::create_symlink(target, dstPath.toStdString(), ec);

            if (ec) {
                return LINGLONG_ERR(QString("Failed to create symlink: %1 -> %2: %3")
                                      .arg(info.filePath())
                                      .arg(target.c_str())
                                      .arg(ec.message().c_str()));
            }
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
        // 如果不以^符号开头，当作普通路径使用
        if (!rule.startsWith("^")) {
            // append $PROJECT_ROOT/output/_build/files to prefix
            rule = QDir::cleanPath(src + "/" + rule);
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
    return LINGLONG_OK;
}

bool runInNamespace()
{
    const int innerUid = 0;
    const int innerGid = 0;
    int outerUid = getuid();
    int outerGid = getgid();

    if (-1 == unshare(CLONE_NEWNS | CLONE_NEWUSER)) {
        perror("unshare");
        return false;
    }

    std::ofstream uidMapFile("/proc/self/uid_map");
    if (!uidMapFile.is_open()) {
        qCritical() << "failed to open uid_map";
        return false;
    }
    std::ostringstream content;
    content << innerUid << " " << outerUid << " 1\n";
    uidMapFile << content.str();
    uidMapFile.close();

    std::ofstream setgroupsFile("/proc/self/setgroups");
    if (!setgroupsFile.is_open()) {
        qCritical() << "failed to open setgroups";
        return false;
    }
    setgroupsFile << "deny";
    setgroupsFile.close();

    std::ofstream gidMapFile("/proc/self/gid_map");
    if (!gidMapFile.is_open()) {
        qCritical() << "failed to open gid_map";
        return false;
    }
    std::ostringstream().swap(content);
    content << innerGid << " " << outerGid << " 1\n";
    gidMapFile << content.str();
    gidMapFile.close();

    return getuid() == innerUid;
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

utils::error::Result<void> Builder::buildStagePrepare() noexcept
{
    LINGLONG_TRACE("build prepare");

    // fuse-overlayfs should run in new user_namespaces and
    // run with CAP_DAC_OVERRIDE capbilities. So we unshare
    // process to new user_namespaces, and map root/root in
    // namespace to current user/group.
    //
    // mount needs CAP_SYS_ADMIN capbilities in the
    // user_namespaces associated with current mount_namespaces,
    // So we also unshare to new mount_namespaces.
    if (!runInNamespace()) {
        return LINGLONG_ERR("failed to run in namespace");
    }

    uid = getuid();
    gid = getgid();

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR("invalid project info", ref);
    }
    projectRef = std::move(ref).value();

    static QRegularExpression appIDReg(
      "[a-zA-Z0-9][-a-zA-Z0-9]{0,62}(\\.[a-zA-Z0-9][-a-zA-Z0-9]{0,62})+");
    auto matches = appIDReg.match(projectRef->id);
    if (not(matches.isValid() && matches.hasMatch())) {
        qWarning() << "This app id does not follow the reverse domain name notation convention. "
                      "See https://wikipedia.org/wiki/Reverse_domain_name_notation";
    }

    if (this->project.package.kind != "runtime") {
        if (project.command.value_or(std::vector<std::string>{}).empty()) {
            return LINGLONG_ERR("command field is required, please specify!");
        }
        installPrefix = QString::fromStdString("/opt/apps/" + this->project.package.id + "/files");
    } else {
        installPrefix = "/runtime";
    }

    if (!QFileInfo::exists(LINGLONG_BUILDER_HELPER)) {
        return LINGLONG_ERR("builder helper doesn't exists");
    }

    printBasicInfo();

    printRepo();

    this->workingDir.mkdir("linglong");

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::buildStageFetchSource() noexcept
{
    LINGLONG_TRACE("build stage fetch source");

    if (!this->project.sources || this->buildOptions.skipFetchSource) {
        return LINGLONG_OK;
    }

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

    return LINGLONG_OK;
}

utils::error::Result<package::Reference> Builder::clearDependency(const std::string &ref,
                                                                  bool onlyLocal) noexcept
{
    LINGLONG_TRACE("clear dependency");

    auto fuzzyRef = package::FuzzyReference::parse(QString::fromStdString(ref));
    if (!fuzzyRef) {
        return LINGLONG_ERR(QString::fromStdString("invalid ref ") + ref.c_str());
    }

    auto res =
      repo.clearReference(*fuzzyRef, { .forceRemote = !onlyLocal, .fallbackToRemote = false });
    if (!res) {
        return LINGLONG_ERR(QString{"ref doesn't exist %1"}.arg(fuzzyRef->toString()));
    }

    return res;
}

utils::error::Result<void> Builder::buildStagePullDependency() noexcept
{
    LINGLONG_TRACE("build stage pull dependency");

    printMessage("[Processing Dependency]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", -35) // NOLINT
                   .arg("Version", -15) // NOLINT
                   .arg("Module", -15)  // NOLINT
                   .arg("Status")
                   .toStdString(),
                 2);

    auto ref = clearDependency(this->project.base, this->buildOptions.skipPullDepend);
    if (!ref) {
        return LINGLONG_ERR("base dependency error", ref);
    }
    baseRef = std::move(ref).value();

    if (this->project.runtime) {
        ref = clearDependency(*this->project.runtime, this->buildOptions.skipPullDepend);
        if (!ref) {
            return LINGLONG_ERR("runtime dependency error", ref);
        }
        runtimeRef = std::move(ref).value();
    }

    if (!this->buildOptions.skipPullDepend) {
        if (!pullDependency(*baseRef, this->repo, "binary")) {
            return LINGLONG_ERR("failed to pull base binary " + baseRef->toString());
        }

        printReplacedText(QString("%1%2%3%4")
                            .arg(baseRef->id, -35)                 // NOLINT
                            .arg(baseRef->version.toString(), -15) // NOLINT
                            .arg("binary", -15)                    // NOLINT
                            .arg("complete\n")
                            .toStdString(),
                          2);

        if (!pullDependency(*baseRef, this->repo, "develop")) {
            return LINGLONG_ERR("failed to pull base develop " + baseRef->toString());
        }

        printReplacedText(QString("%1%2%3%4")
                            .arg(baseRef->id, -35)                 // NOLINT
                            .arg(baseRef->version.toString(), -15) // NOLINT
                            .arg("develop", -15)                   // NOLINT
                            .arg("complete\n")
                            .toStdString(),
                          2);

        if (runtimeRef) {
            if (!pullDependency(*runtimeRef, this->repo, "binary")) {
                return LINGLONG_ERR("failed to pull runtime binary " + runtimeRef->toString());
            }

            printReplacedText(QString("%1%2%3%4")
                                .arg(runtimeRef->id, -35)                 // NOLINT
                                .arg(runtimeRef->version.toString(), -15) // NOLINT
                                .arg("binary", -15)                       // NOLINT
                                .arg("complete\n")
                                .toStdString(),
                              2);

            if (!pullDependency(*runtimeRef, this->repo, "develop")) {
                return LINGLONG_ERR("failed to pull runtime develop " + runtimeRef->toString());
            }

            printReplacedText(QString("%1%2%3%4")
                                .arg(runtimeRef->id, -35)                 // NOLINT
                                .arg(runtimeRef->version.toString(), -15) // NOLINT
                                .arg("develop", -15)                      // NOLINT
                                .arg("complete\n")
                                .toStdString(),
                              2);
        }
    }

    auto ret = this->repo.mergeModules();
    if (!ret) {
        return LINGLONG_ERR("failed to merge modules", ret);
    }

    auto layerDir = this->repo.getMergedModuleDir(*baseRef, false);
    if (!layerDir) {
        return LINGLONG_ERR("failed to get merged base layer", layerDir);
    }
    baseLayerDir = *layerDir;

    if (runtimeRef) {
        layerDir = this->repo.getMergedModuleDir(*runtimeRef, false);
        if (!layerDir) {
            return LINGLONG_ERR("failed to get merged runtime layer", layerDir);
        }

        auto layerItem = this->repo.getLayerItem(*runtimeRef);
        if (!layerItem) {
            return LINGLONG_ERR("failed to get runtime layer item", layerItem);
        }

        auto ref = clearDependency(layerItem->info.base, true);
        if (!ref || *ref != *baseRef) {
            return LINGLONG_ERR("base is not compatible with runtime ");
        }

        runtimeLayerDir = *layerDir;
    }

    return LINGLONG_OK;
}

std::unique_ptr<utils::OverlayFS> Builder::makeOverlay(QString lowerdir,
                                                       QString overlayDir) noexcept
{
    std::unique_ptr<utils::OverlayFS> overlay =
      std::make_unique<utils::OverlayFS>(lowerdir,
                                         overlayDir + "/upperdir",
                                         overlayDir + "/workdir",
                                         overlayDir + "/merged");
    if (!overlay->mount()) {
        return nullptr;
    }

    return overlay;
}

utils::error::Result<void> Builder::processBuildDepends() noexcept
{
    LINGLONG_TRACE("process build depends");

    // processing buildext.apt.buildDepends
    auto res = generateBuildDependsScript();
    if (!res) {
        return LINGLONG_ERR("failed to generate build depends script", res);
    }
    if (!*res) {
        return LINGLONG_OK;
    }

    printMessage("[Processing buildext.apt.buildDepends]");

    auto containerID = runtime::genContainerID(*projectRef);
    auto bundle = runtime::getBundleDir(containerID);
    if (!bundle) {
        return LINGLONG_ERR("failed to get bundle directory", bundle);
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    cfgBuilder.setAppId(this->project.package.id)
      .setBasePath(baseOverlay->mergedDirPath().toStdString(), false)
      .setBundlePath(*bundle)
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindSys()
      .bindProc()
      .bindHostStatics()
      .setExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
        ocppi::runtime::config::types::Mount{ .destination = "/project",
                                              .options = { { "rbind", "ro" } },
                                              .source =
                                                this->workingDir.absolutePath().toStdString(),
                                              .type = "bind" },
      })
      .forwordDefaultEnv();

    if (this->project.package.kind != "runtime") {
        cfgBuilder.setAppPath(buildOutput.path().toStdString(), false);
        if (runtimeLayerDir) {
            cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath().toStdString(), false);
        }
    } else {
        cfgBuilder.setRuntimePath(buildOutput.path().toStdString(), false);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder, QString::fromStdString(containerID));
    if (!container) {
        return LINGLONG_ERR(container);
    }

    auto process = ocppi::runtime::config::types::Process{};
    process.args = { "/bin/bash", "/project/linglong/buildext.sh" };
    process.cwd = "/project";
    process.noNewPrivileges = true;
    process.terminal = true;

    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR("failed to process buildext.apt.buildDepends", result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::buildStagePreBuild() noexcept
{
    LINGLONG_TRACE("build stage pre build");

    // clean output
    QDir(this->workingDir.absoluteFilePath("linglong/output")).removeRecursively();

    buildOutput.setPath(this->workingDir.absoluteFilePath("linglong/output/_build"));
    if (!buildOutput.mkpath(".")) {
        return LINGLONG_ERR("make path " + buildOutput.path() + ": failed.");
    }
    qDebug() << "create develop output success";

    QString overlayPrefix("linglong/overlay");

    // prepare overlayfs
    baseOverlay = makeOverlay(baseLayerDir->absoluteFilePath("files"),
                              this->workingDir.absoluteFilePath(overlayPrefix + "/build_base"));
    if (!baseOverlay) {
        return LINGLONG_ERR("failed to mount build base overlayfs");
    }

    if (runtimeLayerDir) {
        runtimeOverlay =
          makeOverlay(runtimeLayerDir->absoluteFilePath("files"),
                      this->workingDir.absoluteFilePath(overlayPrefix + "/build_runtime"));
        if (!runtimeOverlay) {
            return LINGLONG_ERR("failed to mount build runtime overlayfs");
        }
    }

    auto res = processBuildDepends();
    if (!res) {
        return LINGLONG_ERR("failed to process buildext", res);
    }

    return LINGLONG_OK;
}

utils::error::Result<bool> Builder::buildStageBuild(const QStringList &args) noexcept
{
    LINGLONG_TRACE("build stage build");

    if (this->buildOptions.skipRunContainer) {
        return false;
    }

    utils::error::Result<void> res;
    if (!(res = buildStagePreBuild())) {
        return LINGLONG_ERR("stage pre build failed", res);
    }

    // Set current pgid as foreground process group. If
    // someone takes the foreground process group and dies,
    // We may catch SIGTTIN/SIGTTOU signal
    takeTerminalForeground();

    res = generateEntryScript();
    if (!res) {
        return LINGLONG_ERR("failed to generate entry script", res);
    }

    // initialize the cache dir
    QDir appCache = this->workingDir.absoluteFilePath("linglong/cache");
    if (!appCache.mkpath(".")) {
        return LINGLONG_ERR("make path " + appCache.absolutePath() + ": failed.");
    }

    // write ld.so.conf
    QFile ldsoconf{ appCache.absoluteFilePath("ld.so.conf") };
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
    ldRawConf.replace("@triplet@", projectRef->arch.getTriplet());
    ldsoconf.write(ldRawConf.toUtf8());
    // must be closed here, this conf will be used later.
    ldsoconf.close();

    auto containerID = runtime::genContainerID(*projectRef);
    auto bundle = runtime::getBundleDir(containerID);
    if (!bundle) {
        return LINGLONG_ERR("failed to get bundle directory", bundle);
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    cfgBuilder.setAppId(this->project.package.id)
      .setBasePath(baseOverlay->mergedDirPath().toStdString(), false)
      .setBundlePath(*bundle)
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindSys()
      .bindProc()
      .bindHostStatics()
      .setExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
        ocppi::runtime::config::types::Mount{ .destination = LINGLONG_BUILDER_HELPER,
                                              .options = { { "rbind", "ro" } },
                                              .source = LINGLONG_BUILDER_HELPER,
                                              .type = "bind" },
        ocppi::runtime::config::types::Mount{ .destination = "/project",
                                              .options = { { "rbind", "rw" } },
                                              .source =
                                                this->workingDir.absolutePath().toStdString(),
                                              .type = "bind" },
        ocppi::runtime::config::types::Mount{
          .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
          .options = { { "rbind", "ro" } },
          .source = appCache.absoluteFilePath("ld.so.conf").toStdString(),
          .type = "bind" },
      })
      .setStartContainerHooks(std::vector<ocppi::runtime::config::types::Hook>{
        ocppi::runtime::config::types::Hook{ .args = std::vector<std::string>{ "ldconfig" },
                                             .env = {},
                                             .path = "/sbin/ldconfig",
                                             .timeout = {} } })
      .forwordDefaultEnv()
      .addMask({
        "/project/linglong/output",
        "/project/linglong/overlay",
      });

    if (this->buildOptions.isolateNetWork) {
        cfgBuilder.isolateNetWork();
    }

    if (this->project.package.kind != "runtime") {
        cfgBuilder.setAppPath(buildOutput.path().toStdString(), false);
        if (runtimeLayerDir) {
            cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath().toStdString(), false);
        }
    } else {
        cfgBuilder.setRuntimePath(buildOutput.path().toStdString(), false);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder, QString::fromStdString(containerID));
    if (!container) {
        return LINGLONG_ERR(container);
    }
    qDebug() << "create container success";

    auto arguments = std::vector<std::string>{};
    for (const auto &arg : args) {
        arguments.push_back(arg.toStdString());
    }

    auto process = ocppi::runtime::config::types::Process{};
    process.args = std::move(arguments);
    process.cwd = "/project";
    process.env = { {
      "PREFIX=" + installPrefix.toStdString(),
      "TRIPLET=" + projectRef->arch.getTriplet().toStdString(),
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

    return true;
}

utils::error::Result<void> Builder::buildStagePreCommit() noexcept
{
    LINGLONG_TRACE("build stage pre commit");

    // processing buildext.apt.depends
    auto res = generateDependsScript();
    if (!res) {
        return LINGLONG_ERR("failed to generate depends script", res);
    }
    if (!*res) {
        return LINGLONG_OK;
    }

    takeTerminalForeground();

    QString overlayPrefix("linglong/overlay");

    // clean prepare overlay
    QDir(this->workingDir.absoluteFilePath(overlayPrefix + "prepare_base")).removeRecursively();
    QDir(this->workingDir.absoluteFilePath(overlayPrefix + "prepare_runtime")).removeRecursively();

    // prepare overlay
    baseOverlay = makeOverlay(baseLayerDir->absoluteFilePath("files"),
                              this->workingDir.absoluteFilePath(overlayPrefix + "/prepare_base"));
    if (!baseOverlay) {
        return LINGLONG_ERR("failed to mount prepare base overlayfs");
    }

    if (runtimeLayerDir) {
        runtimeOverlay =
          makeOverlay(runtimeLayerDir->absoluteFilePath("files"),
                      this->workingDir.absoluteFilePath(overlayPrefix + "/prepare_runtime"));
        if (!runtimeOverlay) {
            return LINGLONG_ERR("failed to mount build runtime overlayfs");
        }
    }

    auto containerID = runtime::genContainerID(*projectRef);
    auto bundle = runtime::getBundleDir(containerID);
    if (!bundle) {
        return LINGLONG_ERR("failed to get bundle directory", bundle);
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    cfgBuilder.setAppId(this->project.package.id)
      .setBasePath(baseOverlay->mergedDirPath().toStdString(), false)
      .setBundlePath(*bundle)
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindSys()
      .bindProc()
      .bindHostStatics()
      .setExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
        ocppi::runtime::config::types::Mount{ .destination = "/project",
                                              .options = { { "rbind", "rw" } },
                                              .source =
                                                this->workingDir.absolutePath().toStdString(),
                                              .type = "bind" },
      })
      .forwordDefaultEnv();

    if (this->project.package.kind != "runtime") {
        cfgBuilder.setAppPath(buildOutput.path().toStdString(), false);
        if (runtimeLayerDir) {
            cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath().toStdString(), false);
        }
    } else {
        cfgBuilder.setRuntimePath(buildOutput.path().toStdString(), false);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder, QString::fromStdString(containerID));
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process process;
    process.args = std::vector{ std::string("bash"), std::string("/project/linglong/buildext.sh") };
    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    qDebug() << "install depends success";

    // merge subdirectory
    // 1. merge base to runtime, Or
    // 2. merge base and runtime to app,
    // base prefix is /usr, and runtime prefix is /runtime
    QList<QDir> src = { baseOverlay->upperDirPath() + "/usr" };
    if (this->project.package.kind == "app") {
        src.append(runtimeOverlay->upperDirPath());
    }
    mergeOutput(src, buildOutput, QStringList({ "bin/", "lib/" }));

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateAppConf() noexcept
{
    LINGLONG_TRACE("generate application configure");

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

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::installFiles() noexcept
{
    LINGLONG_TRACE("install files");

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
        if (!ret) {
            return LINGLONG_ERR("failed to install full develop files", ret);
        }
    }

    std::vector<api::types::v1::BuilderProjectModules> projectModules;
    auto hasDevelop = false;
    auto hasBinary = false;
    if (this->project.modules.has_value()) {
        for (const auto &module : *this->project.modules) {
            if (module.name == "develop") {
                hasDevelop = true;
            } else if (module.name == "binary") {
                hasBinary = true;
            }
            projectModules.push_back(module);
            packageModules.push_back(module.name.c_str());
        }
    }
    if (!hasBinary) {
        packageModules.push_back("binary");
    }
    if (!hasDevelop) {
        packageModules.push_back("develop");
        // 如果没有develop模块，则添加一个默认的，包含include/**, lib/debug/**, lib/**.a三种匹配
        if (!this->buildOptions.fullDevelop) {
            projectModules.push_back(api::types::v1::BuilderProjectModules{
              .files = { "^/include/.+", "^/lib/debug/.+", "^/lib/.+\\.a$" },
              .name = "develop",
            });
        }
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
        auto ret = installModule(installRules, buildOutput.path(), moduleDir.filePath("files"));
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
        auto ret = installModule(installRules, buildOutput.path(), moduleDir.filePath("files"));
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

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateEntries() noexcept
{
    LINGLONG_TRACE("generate entries");

    printMessage("");

    if (this->project.package.kind == "runtime") {
        return LINGLONG_OK;
    }

    qDebug() << "generate entries";
    // TODO: The current whitelist logic is not very flexible.
    // The application configuration file can be exported after configuring it in the build
    // configuration file(linglong.yaml).
    // 仅导出名单中的目录，以避免意外文件影响系统功能
    const std::filesystem::path exportDirConfigPath = LINGLONG_DATA_DIR "/export-dirs.json";
    if (!std::filesystem::exists(exportDirConfigPath)) {
        return LINGLONG_ERR(QString{ "this export config file doesn't exist: %1" }.arg(
          exportDirConfigPath.c_str()));
    }
    auto exportDirConfig =
      linglong::utils::serialize::LoadJSONFile<linglong::api::types::v1::ExportDirs>(
        exportDirConfigPath);
    if (!exportDirConfig) {
        return LINGLONG_ERR(
          QString{ "failed to load export config file: %1" }.arg(exportDirConfigPath.c_str()));
    }

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

    for (const auto &exportPath : exportDirConfig->exportPaths) {
        const auto &path = QString::fromStdString(exportPath);
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
        // 2025-03-24 修订
        // XDG_DATA_DIR的优先级高于/usr/lib/systemd，如果应用意外导出了系统服务文件，例如dbus.service，会导致系统功能异常
        // 现在安装应用时会通过generator脚本将lib/systemd/user下的文件复制到优先级最低的generator.late目录
        // 因此构建时将files/lib/systemd/user的内容复制到entries/lib/systemd/user
        if (!binaryEntries.mkpath("lib/systemd/user")) {
            qWarning() << "mkpath files/lib/systemd/user: failed";
        }
        auto ret = copyDir(binaryFiles.filePath("lib/systemd/user"),
                           binaryEntries.absoluteFilePath("lib/systemd/user"));
        if (!ret.has_value()) {
            return LINGLONG_ERR(ret);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::commitToLocalRepo() noexcept
{
    LINGLONG_TRACE("commit to local repo");

    auto appIDPrintWidth = -this->project.package.id.size() + -5;

    auto info = api::types::v1::PackageInfoV2{
        .arch = { projectRef->arch.toString().toStdString() },
        .channel = projectRef->channel.toStdString(),
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

    auto fuzzyBase = package::FuzzyReference::parse(QString::fromStdString(this->project.base));
    // when the base version is likes 20.0.0.1, warning that it is a full version
    // if the base version is likes 20.0.0, we should also write 20.0.0 to info.json
    if (fuzzyBase->version->tweak) {
        qWarning() << fuzzyBase->toString() << "is set a full version.";
    } else {
        baseRef->version.tweak = std::nullopt;
    }
    info.base = baseRef->toString().toStdString();
    if (runtimeRef) {
        auto fuzzyRuntime =
          package::FuzzyReference::parse(QString::fromStdString(*this->project.runtime));
        // the runtime version is same as base.
        if (fuzzyRuntime->version->tweak) {
            qWarning() << fuzzyRuntime->toString() << "is set a full version.";
        } else {
            runtimeRef->version.tweak = std::nullopt;
        }
        info.runtime = runtimeRef->toString().toStdString();
    }
    // 从本地仓库清理旧的ref
    auto existsModules = this->repo.getModuleList(*projectRef);
    for (const auto &module : existsModules) {
        auto result = this->repo.remove(*projectRef, module);
        if (!result) {
            qDebug() << "remove" << projectRef->toString() << result.error().message();
        }
    }
    // 推送新的ref到本地仓库
    printMessage("[Commit Contents]");
    printMessage(QString("%1%2%3%4")
                   .arg("Package", appIDPrintWidth) // NOLINT
                   .arg("Version", -15)             // NOLINT
                   .arg("Module", -15)              // NOLINT
                   .arg("Status")
                   .toStdString(),
                 2);
    for (const auto &module : packageModules) {
        QDir moduleOutput = this->workingDir.absoluteFilePath("linglong/output/" + module);
        info.packageInfoV2Module = module.toStdString();
        auto ret =
          linglong::utils::calculateDirectorySize(moduleOutput.absolutePath().toStdString());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        info.size = static_cast<int>(*ret);

        QFile infoFile{ moduleOutput.filePath("info.json") };
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
    if (!mergeRet) {
        return mergeRet;
    }

    return LINGLONG_OK;
}

utils::error::Result<bool> Builder::buildStageCommit() noexcept
{
    LINGLONG_TRACE("build stage commit");

    if (this->buildOptions.skipCommitOutput) {
        return false;
    }

    utils::error::Result<void> res;
    if (!(res = buildStagePreCommit())) {
        return LINGLONG_ERR("stage pre commit failed", res);
    }

    if (!(res = generateAppConf())) {
        return LINGLONG_ERR("failed to generate app conf", res);
    }

    if (!(res = installFiles())) {
        return LINGLONG_ERR("failed to install files", res);
    }

    if (!(res = generateEntries())) {
        return LINGLONG_ERR("failed to generate entries", res);
    }

    if (!(res = commitToLocalRepo())) {
        return LINGLONG_ERR("failed to commit to local repo", res);
    }

    return true;
}

utils::error::Result<void> Builder::build(const QStringList &args) noexcept
{
    LINGLONG_TRACE(
      QString("build project %1").arg(this->workingDir.absoluteFilePath("linglong.yaml")));

    utils::error::Result<void> res;
    if (!(res = buildStagePrepare())) {
        return LINGLONG_ERR("stage prepare error", res);
    }

    if (!(res = buildStageFetchSource())) {
        return LINGLONG_ERR("stage fetch srouce error", res);
    }

    if (!(res = buildStagePullDependency())) {
        return LINGLONG_ERR("stage pull dependency error", res);
    }

    utils::error::Result<bool> success;
    if (!(success = buildStageBuild(args))) {
        return LINGLONG_ERR("stage build error", success);
    }
    // skip build stage means skip all following stage
    if (!*success) {
        return LINGLONG_OK;
    }

    if (!(success = buildStageCommit())) {
        return LINGLONG_ERR("stage commit error", success);
    }
    // skip commit stage
    if (!*success) {
        return LINGLONG_OK;
    }

    printMessage("Successfully build " + this->project.package.id);

    if (!(res = runtimeCheck())) {
        return LINGLONG_ERR("stage runtime check error", res);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportUAB(const ExportOption &option)
{
    LINGLONG_TRACE("export uab file");

    package::UABPackager packager{ workingDir };

    if (!option.iconPath.empty()) {
        if (auto ret = packager.setIcon(QFileInfo{ option.iconPath.c_str() }); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (project.exclude) {
        packager.exclude(project.exclude.value());
    }

    if (project.include) {
        packager.include(project.include.value());
    }

    if (!option.full && project.runtime) {
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

    if (!option.compressor.empty()) {
        packager.setCompressor(option.compressor.c_str());
    }
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

    if (!option.loader.empty()) {
        packager.setLoader(option.loader.c_str());
    }

    auto uabFile = QString{ "%1_%2_%3_%4.uab" }.arg(curRef->id,
                                                    curRef->arch.toString(),
                                                    curRef->version.toString(),
                                                    curRef->channel);
    if (auto ret = packager.pack(uabFile, !option.full); !ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportLayer(const QString &compressor,
                                                const bool &noExportDevelop)
{
    LINGLONG_TRACE("export layer file");

    auto ref = currentReference(this->project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto modules = this->repo.getModuleList(*ref);

    package::LayerPackager pkger;
    if (!compressor.isEmpty()) {
        pkger.setCompressor(compressor);
    }
    for (const auto &module : modules) {
        if (noExportDevelop && module == "develop") {
            continue;
        }

        auto layerDir = this->repo.getLayerDir(*ref, module);
        if (!layerDir) {
            qCritical().nospace() << "resolve layer " << ref->toString() << "/" << module.c_str()
                                  << " failed: " << layerDir.error().message();
            continue;
        }

        auto layerFile = QString("%1/%2_%3_%4_%5.layer")
                           .arg(workingDir.absolutePath(),
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
                                        bool debug)
{
    LINGLONG_TRACE("run application");
    auto curRef = currentReference(this->project);
    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    auto containerID = runtime::genContainerID(*curRef);
    auto bundle = runtime::getBundleDir(containerID);
    if (!bundle) {
        return LINGLONG_ERR(bundle);
    }

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

    utils::error::Result<package::LayerDir> runtimeDir;
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
        runtimeDir = debug ? this->repo.getMergedModuleDir(*runtimeRef)
                           : this->repo.getLayerDir(*runtimeRef, "binary");
        if (!runtimeDir) {
            return LINGLONG_ERR(runtimeDir);
        }
    }

    utils::error::Result<package::LayerDir> curDir;
    // mergedDir 会自动在释放时删除临时目录，所以要用变量保留住
    utils::error::Result<std::shared_ptr<package::LayerDir>> mergedDir;
    if (modules.size() > 1) {
        qDebug() << "create temp merge dir."
                 << "ref: " << curRef->toString() << "modules: " << modules;
        mergedDir = this->repo.getMergedModuleDir(*curRef, modules);
        if (!mergedDir.has_value()) {
            return LINGLONG_ERR(mergedDir);
        }
        curDir = **mergedDir;
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

    if (this->project.package.kind != "app") {
        return LINGLONG_ERR("only app can run");
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
      [&applicationMounts,
       &bundle](const api::types::v1::ApplicationConfigurationPermissionsInnerBind &bind) {
          applicationMounts.push_back(ocppi::runtime::config::types::Mount{
            .destination = bind.destination,
            .gidMappings = {},
            .options = { { "rbind" } },
            .source = bundle->string() + "/rootfs" + bind.source,
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

    auto *homeEnv = ::getenv("HOME");
    auto *userNameEnv = ::getenv("USER");
    if (homeEnv == nullptr || userNameEnv == nullptr) {
        return LINGLONG_ERR("Couldn't get HOME or USER from env.");
    }

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

            applicationMounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = std::string("/home/") + userNameEnv + "/.gdbinit",
              .options = { { "ro", "rbind" } },
              .source = gdbinit,
              .type = "bind",
            });
        }
    }

    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/project",
      .options = { { "rbind", "rw" } },
      .source = this->workingDir.absolutePath().toStdString(),
      .type = "bind",
    });

    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = LINGLONG_BUILDER_HELPER,
      .options = { { "rbind", "ro" } },
      .source = LINGLONG_BUILDER_HELPER,
      .type = "bind",
    });

    QDir appCache = this->workingDir.absoluteFilePath("linglong/cache");
    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
      .options = { { "rbind", "ro" } },
      .source = appCache.absoluteFilePath("ld.so.conf").toStdString(),
      .type = "bind",
    });

    std::vector<ocppi::runtime::config::types::Hook> startContainer{
        {
          .args =
            std::vector<std::string>{ "/sbin/ldconfig", "-C", "/run/linglong/cache/ld.so.cache" },
          .env = {},
          .path = "/sbin/ldconfig",
          .timeout = {},
        },
    };

    int64_t uid = getuid();
    int64_t gid = getgid();

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    cfgBuilder.setAppId(curRef->id.toStdString())
      .setAppPath(curDir->absoluteFilePath("files").toStdString())
      .setBasePath(baseDir->absoluteFilePath("files").toStdString())
      .setAppCache(appCache.absolutePath().toStdString(), false)
      .enableLDCache()
      .setBundlePath(std::move(bundle).value())
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindSys()
      .bindProc()
      .bindDev()
      .bindDevNode()
      .bindCgroup()
      .bindRun()
      .bindTmp()
      .bindUserGroup()
      .bindMedia()
      .bindHostRoot()
      .bindHostStatics()
      .bindHome(homeEnv, userNameEnv)
      .enablePrivateDir()
      .mapPrivate(std::string("/home/") + userNameEnv + "/.ssh", true)
      .mapPrivate(std::string("/home/") + userNameEnv + "/.gnupg", true)
      .bindIPC()
      .forwordDefaultEnv()
      .setExtraMounts(applicationMounts)
      .setStartContainerHooks(std::move(startContainer))
      .enableSelfAdjustingMount();
    if (this->project.runtime) {
        cfgBuilder.setRuntimePath(runtimeDir->absoluteFilePath("files").toStdString());
    }
#ifdef LINGLONG_FONT_CACHE_GENERATOR
    cfgBuilder.enableFontCache();
#endif

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder, QString::fromStdString(containerID));

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
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::runtimeCheck()
{
    LINGLONG_TRACE("runtime check");
    printMessage("[Runtime Check]");
    // Do some checks after run container
    if (!this->buildOptions.skipCheckOutput && this->project.package.kind == "app") {
        printMessage("Start runtime check", 2);
        auto ret =
          this->run(packageModules, { { QString{ LINGLONG_BUILDER_HELPER } + "/main-check.sh" } });
        if (!ret) {
            printMessage("Runtime check failed", 2);
            return LINGLONG_ERR(ret);
        }
    } else {
        printMessage("Skip runtime check", 2);
    }

    printMessage("Runtime check done", 2);
    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateEntryScript() noexcept
{
    LINGLONG_TRACE("generate entry script");

    QFile entry{ this->workingDir.absoluteFilePath("linglong/entry.sh") };
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
    if (!this->buildOptions.skipStripSymbols) {
        scriptContent.append("# POST BUILD PROCESS\n");
        scriptContent.append(LINGLONG_BUILDER_HELPER "/symbols-strip.sh\n");
    }

    auto writeBytes = scriptContent.size();
    auto bytesWrite = entry.write(scriptContent.c_str());

    if (bytesWrite == -1 || (qint64)writeBytes != bytesWrite) {
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

    return LINGLONG_OK;
}

utils::error::Result<bool> Builder::generateBuildDependsScript() noexcept
{
    LINGLONG_TRACE("generate build depends script");

    if (this->project.buildext) {
        std::string content;

        auto &buildext = *this->project.buildext;
        if (buildext.apt) {
            auto &apt = *buildext.apt;
            if (apt.buildDepends) {
                std::string packages;

                for (auto &package : *apt.buildDepends) {
                    packages.append(" ");
                    packages.append(package);
                }

                if (!packages.empty()) {
                    // ll-box current support single-user mode only, force apt use user
                    content.append("echo 'APT::Sandbox::User \"root\";' > "
                                   "/etc/apt/apt.conf.d/99linglong-builder.conf\n");
                    content.append("apt update\n");
                    content.append("apt -y install");
                    content.append(packages);
                    content.append("\n");
                }
            }
        }

        if (!content.empty()) {
            auto res = utils::writeFile(
              this->workingDir.absoluteFilePath("linglong/buildext.sh").toStdString(),
              content);

            return res ? utils::error::Result<bool>(true) : LINGLONG_ERR(res);
        }
    }

    return false;
}

utils::error::Result<bool> Builder::generateDependsScript() noexcept
{
    LINGLONG_TRACE("generate depends script");

    if (this->project.buildext) {
        std::string content;

        auto &buildext = *this->project.buildext;
        if (buildext.apt) {
            auto &apt = *buildext.apt;
            if (apt.depends) {
                std::string packages;

                for (auto &package : *apt.depends) {
                    packages.append(" ");
                    packages.append(package);
                }

                if (!packages.empty()) {
                    content.append("apt -o APT::Sandbox::User=root update || echo \"$?\"\n");
                    content.append("apt -o APT::Sandbox::User=root -y install");
                    content.append(packages);
                    content.append(" || echo \"$?\"\n");
                }
            }
        }

        if (!content.empty()) {
            auto res = utils::writeFile(
              this->workingDir.absoluteFilePath("linglong/buildext.sh").toStdString(),
              content);

            return res ? utils::error::Result<bool>(true) : LINGLONG_ERR(res);
        }
    }

    return false;
}

void Builder::takeTerminalForeground()
{
    struct sigaction sa = { 0 };
    sa.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sa, NULL);

    int pgid = getpgid(0);
    int tty = open("/dev/tty", O_RDONLY);
    if (tty != -1) {
        if (pgid != -1) {
            tcsetpgrp(tty, pgid);
        }

        if (tcgetpgrp(tty) != pgid) {
            qWarning() << pgid << "is not terminal foreground process group";
        }

        close(tty);
    }
}

void Builder::patchBuildPhaseConfig(ocppi::runtime::config::types::Config &config)
{
    if (config.mounts) {
        auto &mounts = config.mounts.value();
        for (auto it = mounts.begin(); it != mounts.end();) {
            bool is_ro = false;
            std::vector<std::string>::iterator ro{};
            if (it->options) {
                auto &options = it->options.value();
                ro = std::find_if(options.begin(), options.end(), [](std::string &option) {
                    return option == "ro";
                });
                is_ro = (ro != options.end());
            }

            if (it->destination == "/runtime") {
                if (is_ro) {
                    *ro = "rw";
                }
            } else if (it->destination.find(LINGLONG_BUILDER_HELPER) == 0) {
            } else if (it->destination == "/etc/ld.so.cache"
                       || (is_ro && it->source.has_value()
                           && it->source.value() == it->destination)) {
                it = mounts.erase(it);
                continue;
            }
            ++it;
        }
    }
}

void Builder::mergeOutput(const QList<QDir> &src, const QDir &dest, const QStringList &targets)
{
    QMap<QString, QString> copys;
    for (auto &dir : src) {
        qDebug() << "mergeOutput " << dir.absolutePath();
        if (!dir.exists())
            continue;

        QDirIterator iter(dir.absolutePath(),
                          QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                          QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            QString path = iter.next();

            if (iter.fileName().startsWith(".wh."))
                continue;

            struct stat st;
            if (-1 == lstat(path.toStdString().c_str(), &st))
                continue;

            if (st.st_size == 0 && st.st_rdev == 0 && st.st_mode == S_IFCHR)
                continue;

            QString relativePath = dir.relativeFilePath(path);

            if (dest.exists(relativePath))
                continue;

            bool found = false;
            for (auto &target : targets) {
                if (relativePath.startsWith(target))
                    found = true;
            }

            if (!found)
                continue;

            copys[relativePath] = std::move(path);
        }
    }

    for (auto it = copys.constBegin(); it != copys.constEnd(); ++it) {

        QFileInfo from(it.value());
        QFileInfo to(dest.filePath(it.key()));

        qDebug() << from.absoluteFilePath() << "->" << to.absoluteFilePath();

        if (from.isDir()) {
            if (!QDir(to.absoluteFilePath()).mkpath(".")) {
                qWarning() << "failed to mkpath " << to.absoluteFilePath();
            }
        } else if (from.isSymLink()) {
            if (!QDir(to.absolutePath()).mkpath(".")) {
                qWarning() << "failed to mkpath " << to.absolutePath();
                continue;
            }

            std::array<char, PATH_MAX + 1> buf{};
            auto size =
              readlink(from.absoluteFilePath().toStdString().c_str(), buf.data(), PATH_MAX);
            if (size == -1) {
                qWarning() << "readlink failed " << from.absoluteFilePath();
                continue;
            }

            if (!QFile::link(QString::fromUtf8(buf.data()), to.absoluteFilePath())) {
                qWarning() << QString("failed to link from %1 to %2")
                                .arg(to.absoluteFilePath())
                                .arg(QString::fromUtf8(buf.data()));
            }
        } else if (from.isFile()) {
            if (!QDir(to.absolutePath()).mkpath(".")) {
                qWarning() << "failed to mkpath " << to.absolutePath();
                continue;
            }

            if (!QFile::copy(from.absoluteFilePath(), to.absoluteFilePath())) {
                qWarning() << QString("failed to copy %1 to %2")
                                .arg(from.absoluteFilePath())
                                .arg(to.absoluteFilePath());
            }
        }
    }
}

void Builder::printBasicInfo()
{
    printMessage("[Build Target]");
    printMessage(this->project.package.id, 2);
    printMessage("[Project Info]");
    printMessage("Package Name: " + this->project.package.name, 2);
    printMessage("Version: " + this->project.package.version, 2);
    printMessage("Package Type: " + this->project.package.kind, 2);
    printMessage("Build Arch: " + projectRef->arch.toString().toStdString(), 2);
}

void Builder::printRepo()
{
    auto repoCfg = this->repo.getConfig();
    printMessage("[Current Repo]");
    printMessage("Name: " + repoCfg.defaultRepo, 2);
    std::string repoUrl;
    const auto &defaultRepo =
      std::find_if(repoCfg.repos.begin(), repoCfg.repos.end(), [&repoCfg](const auto &repo) {
          return repo.alias.value_or(repo.name) == repoCfg.defaultRepo;
      });
    repoUrl = defaultRepo->url;
    printMessage("Url: " + repoUrl, 2);
}

} // namespace linglong::builder
