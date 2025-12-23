/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong_builder.h"

#include "configure.h"
#include "linglong/api/types/v1/ExportDirs.hpp"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/printer.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/package/architecture.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/reference.h"
#include "linglong/package/uab_packager.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container.h"
#include "linglong/utils/command/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"
#include "ocppi/runtime/RunOption.hpp"
#include "source_fetcher.h"

#include <nlohmann/json.hpp>
#include <qdebug.h>

#include <QDir>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>

namespace linglong::builder {

std::vector<std::string> Builder::privilegeBuilderCaps = {
    "CAP_CHOWN",   "CAP_DAC_OVERRIDE",     "CAP_FOWNER",     "CAP_FSETID",
    "CAP_KILL",    "CAP_NET_BIND_SERVICE", "CAP_SETFCAP",    "CAP_SETGID",
    "CAP_SETPCAP", "CAP_SETUID",           "CAP_SYS_CHROOT",
};

namespace {

utils::error::Result<package::Reference>
currentReference(const api::types::v1::BuilderProject &project)
{
    return package::Reference::fromBuilderProject(project);
}

utils::error::Result<void>
fetchSources(const std::vector<api::types::v1::BuilderProjectSource> &sources,
             const QDir &cacheDir,
             const QDir &destination,
             [[maybe_unused]] const api::types::v1::BuilderConfig &cfg) noexcept
{
    LINGLONG_TRACE("fetch sources to " + destination.absolutePath().toStdString());

    for (decltype(sources.size()) pos = 0; pos < sources.size(); ++pos) {
        if (!sources.at(pos).url.has_value()) {
            return LINGLONG_ERR("source missing url");
        }
        auto url = QString::fromStdString(*(sources.at(pos).url));
        if (url.length() > 75) {         // NOLINT
            url = "..." + url.right(70); // NOLINT
        }
        printReplacedText(fmt::format("{:<20}{:<15}{:<75}downloading ...",
                                      "Source " + std::to_string(pos),
                                      sources.at(pos).kind,
                                      url.toStdString()),
                          2);
        SourceFetcher fetcher(sources.at(pos), cacheDir);
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

    service::PackageTask tmpTask({});
    auto partChanged = [&ref, module](const uint fetched, const uint requested) {
        auto percentage = (uint)((((double)fetched) / requested) * 100);
        auto progress = fmt::format("({}/{} {}%)", fetched, requested, percentage);
        printReplacedText(fmt::format("{:<35}{:<15}{:<15}{:<15} {:<15}",
                                      ref.id,
                                      ref.version.toString(),
                                      module,
                                      "downloading",
                                      progress),
                          2);
    };

    QObject::connect(&tmpTask, &service::PackageTask::PartChanged, partChanged);
    printReplacedText(
      fmt::format("{:<35}{:<15}{:<15}waiting ...", ref.id, ref.version.toString(), module),
      2);
    QObject::connect(utils::global::GlobalTaskControl::instance(),
                     &utils::global::GlobalTaskControl::OnCancel,
                     [&tmpTask]() {
                         tmpTask.Cancel();
                     });
    auto res = repo.pull(tmpTask, ref, module, repo.getDefaultRepo());
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

} // namespace

namespace detail {
void mergeOutput(const std::vector<std::filesystem::path> &src,
                 const std::filesystem::path &dest,
                 const std::vector<std::string> &targets)
{
    for (const auto &dir : src) {
        LogD("merge {} to {}", dir, dest);
        auto matcher = [&dir, &targets](const std::filesystem::path &path) {
            if (common::strings::starts_with(path.filename().string(), ".wh.")) {
                return false;
            }

            struct stat st;
            if (-1 == lstat((dir / path).c_str(), &st)) {
                return false;
            }
            if (st.st_size == 0 && st.st_rdev == 0 && st.st_mode == S_IFCHR) {
                return false;
            }

            for (const auto &target : targets) {
                if (common::strings::starts_with(path.string(), target)) {
                    return true;
                }
            }

            return false;
        };

        utils::copyDirectory(dir, dest, matcher);
    }
}

} // namespace detail

// install module files by rules
// files will be moved from buildOutput to moduleOutput
utils::error::Result<std::vector<std::filesystem::path>>
installModule(const std::filesystem::path &buildOutput,
              const std::filesystem::path &moduleOutput,
              const std::unordered_set<std::string> &rules)
{
    LINGLONG_TRACE("install module file");

    auto ret = utils::moveFiles(
      buildOutput,
      moduleOutput,
      [&buildOutput, &rules](const std::filesystem::path &path) {
          for (auto rule : rules) {
              LogD("{} - {}", path, rule);
              // skip empty rule and comment start with #<space>
              if (rule.empty() || (rule.size() > 1 && rule[0] == '#' && rule[1] == ' ')) {
                  continue;
              }

              if (rule.rfind("^", 0) != 0) {
                  // if rule is not start with ^, it's a normal path
                  if (rule[0] == '/') {
                      rule = rule.substr(1);
                  }
                  auto rulePath = buildOutput / rule;
                  auto relativePath = (buildOutput / path).lexically_relative(rulePath);
                  if (!relativePath.empty() && relativePath.string().rfind("..", 0) != 0) {
                      return true;
                  }
              } else {
                  // rule is a regex
                  if (rule.rfind("^/", 0) != 0) {
                      rule.insert(1, buildOutput.string() + "/");
                  } else {
                      rule.insert(1, buildOutput);
                  }

                  QRegularExpression regexp(rule.c_str());
                  if (regexp.match((buildOutput / path).c_str()).hasMatch()) {
                      return true;
                  }
              }
          }
          return false;
      });

    if (!ret) {
        return LINGLONG_ERR("failed to move files", ret);
    }

    return utils::getFiles(moduleOutput);
}

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
        refs.push_back(ref->toString());
    }
    std::sort(refs.begin(), refs.end());
    auto it = std::unique(refs.begin(), refs.end());
    refs.erase(it, refs.end());
    for (const auto &ref : refs) {
        std::cout << ref << std::endl;
    }
    return LINGLONG_OK;
}

utils::error::Result<void> cmdRemoveApp(repo::OSTreeRepo &repo,
                                        std::vector<std::string> refs,
                                        bool prune)
{
    for (const auto &ref : refs) {
        auto r = package::Reference::parse(ref);
        if (!r.has_value()) {
            std::cerr << ref << ": " << r.error().message() << std::endl;
            continue;
        }
        auto modules = repo.getModuleList(*r);
        for (const auto &module : modules) {
            auto v = repo.remove(*r, module);
            if (!v.has_value()) {
                std::cerr << ref << ": " << v.error().message() << std::endl;
                continue;
            }
        }
    }
    if (prune) {
        auto v = repo.prune();
        if (!v.has_value()) {
            std::cerr << v.error().message();
        }
    }
    auto v = repo.mergeModules();
    if (!v.has_value()) {
        std::cerr << v.error().message();
    }
    return LINGLONG_OK;
}

Builder::Builder(std::optional<api::types::v1::BuilderProject> project,
                 std::filesystem::path projectDir,
                 repo::OSTreeRepo &repo,
                 runtime::ContainerBuilder &containerBuilder,
                 const api::types::v1::BuilderConfig &cfg)
    : repo(repo)
    , workingDir(std::move(projectDir))
    , project(project)
    , containerBuilder(containerBuilder)
    , cfg(cfg)
    , buildContext(repo)
{
    LogD("builder working dir: {}", workingDir);
    internalDir = workingDir / "linglong";
}

auto Builder::getConfig() const noexcept -> api::types::v1::BuilderConfig
{
    return this->cfg;
}

void Builder::setConfig(const api::types::v1::BuilderConfig &cfg) noexcept
{
    if (this->cfg.repo != cfg.repo) {
        LogE("update ostree repo path of builder is not supported.");
        std::abort();
    }

    this->cfg = cfg;
}

utils::error::Result<void> Builder::buildStagePrepare() noexcept
{
    LINGLONG_TRACE("build prepare");

    uid = getuid();
    gid = getgid();

    auto ref = currentReference(this->project.value());
    if (!ref) {
        return LINGLONG_ERR("invalid project info", ref);
    }
    projectRef = std::move(ref).value();

    static QRegularExpression appIDReg(
      "[a-zA-Z0-9][-a-zA-Z0-9]{0,62}(\\.[a-zA-Z0-9][-a-zA-Z0-9]{0,62})+");
    auto matches = appIDReg.match(QString::fromStdString(projectRef->id));
    if (not(matches.isValid() && matches.hasMatch())) {
        LogW("This app id does not follow the reverse domain name notation convention. "
             "See https://wikipedia.org/wiki/Reverse_domain_name_notation");
    }

    if (this->project->package.kind == "app") {
        if (project->command.value_or(std::vector<std::string>{}).empty()) {
            return LINGLONG_ERR("command field is required, please specify!");
        }
        installPrefix = "/opt/apps/" + this->project->package.id + "/files";
    } else if (this->project->package.kind == "runtime") {
        installPrefix = "/runtime";
    } else if (this->project->package.kind == "extension") {
        installPrefix = "/opt/extensions/" + this->project->package.id;
    }

    if (!QFileInfo::exists(LINGLONG_BUILDER_HELPER)) {
        return LINGLONG_ERR("builder helper doesn't exists");
    }

    if (!checkDeprecatedInstallFile()) {
        return LINGLONG_ERR("$appid.install is deprecated, please use modules instead. see "
                            "https://linglong.space/guide/ll-builder/modules.html");
    }

    printBasicInfo();

    printRepo();

    std::error_code ec;
    std::filesystem::create_directory(this->internalDir, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to create internal dir {} {}", this->internalDir, ec.message())
            .c_str());
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::buildStageFetchSource() noexcept
{
    LINGLONG_TRACE("build stage fetch source");

    if (!this->project->sources || this->buildOptions.skipFetchSource) {
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
    auto fetchCacheDir = QString::fromStdString(internalDir / "cache");
    if (!qEnvironmentVariableIsEmpty("LINGLONG_FETCH_CACHE")) {
        fetchCacheDir = qgetenv("LINGLONG_FETCH_CACHE");
    }
    // clean sources directory on every build
    auto fetchSourcesDir = QDir(QString::fromStdString(internalDir / "sources"));
    fetchSourcesDir.removeRecursively();
    auto result = fetchSources(*this->project->sources, fetchCacheDir, fetchSourcesDir, this->cfg);

    if (!result) {
        return LINGLONG_ERR(result);
    }

    return LINGLONG_OK;
}

utils::error::Result<package::Reference> Builder::ensureUtils(const std::string &id) noexcept
{
    LINGLONG_TRACE("ensureUtils");

    // always try to get newest version from remote
    auto ref = clearDependency(id, true, true);
    auto localRef = clearDependency(id, false, false);
    if (localRef) {
        if (!ref || localRef->version > ref->version) {
            ref = std::move(localRef);
        }
    }
    if (ref && pullDependency(*ref, this->repo, "binary")) {
        auto appLayerDir = this->repo.getMergedModuleDir(*ref);
        if (!appLayerDir) {
            return LINGLONG_ERR("failed to get layer dir of " + ref->toString());
        }

        auto layerItem = this->repo.getLayerItem(*ref);
        if (!layerItem) {
            return LINGLONG_ERR("failed to get layer item of " + ref->toString());
        }
        const auto &info = layerItem->info;

        auto baseRef = clearDependency(info.base, false, true);
        if (!baseRef) {
            return LINGLONG_ERR("base not exist: " + QString::fromStdString(info.base));
        }
        if (!pullDependency(*baseRef, this->repo, "binary")) {
            return LINGLONG_ERR("failed to pull base binary " + QString::fromStdString(info.base));
        }

        if (info.runtime) {
            auto runtimeRef = clearDependency(info.runtime.value(), false, true);
            if (!runtimeRef) {
                return LINGLONG_ERR("runtime not exist: "
                                    + QString::fromStdString(info.runtime.value()));
            }
            if (!pullDependency(*runtimeRef, this->repo, "binary")) {
                return LINGLONG_ERR("failed to pull runtime binary "
                                    + QString::fromStdString(info.runtime.value()));
            }
        }

        return ref;
    }

    return LINGLONG_ERR("failed to get utils " + QString::fromStdString(id));
}

utils::error::Result<package::Reference> Builder::clearDependency(const std::string &ref,
                                                                  bool forceRemote,
                                                                  bool fallbackToRemote) noexcept
{
    LINGLONG_TRACE("clear dependency");

    auto fuzzyRef = package::FuzzyReference::parse(ref);
    if (!fuzzyRef) {
        return LINGLONG_ERR(QString::fromStdString("invalid ref ") + ref.c_str());
    }

    auto res = repo.clearReference(*fuzzyRef,
                                   { .forceRemote = forceRemote,
                                     .fallbackToRemote = fallbackToRemote,
                                     .semanticMatching = true });
    if (!res) {
        return LINGLONG_ERR(fmt::format("ref doesn't exist {}", fuzzyRef->toString()));
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

    auto baseRef = clearDependency(this->project->base, !this->buildOptions.skipPullDepend, false);
    if (!baseRef) {
        return LINGLONG_ERR("base dependency error", baseRef);
    }

    std::optional<package::Reference> runtimeRef;
    if (this->project->runtime) {
        auto ref =
          clearDependency(*this->project->runtime, !this->buildOptions.skipPullDepend, false);
        if (!ref) {
            return LINGLONG_ERR("runtime dependency error", ref);
        }
        runtimeRef = std::move(ref).value();
    }

    if (!this->buildOptions.skipPullDepend) {
        auto ref = pullDependency(*baseRef, this->repo, "binary");
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to pull base binary " + baseRef->toString(), ref);
        }

        printReplacedText(fmt::format("{:<35}{:<15}{:<15}complete\n",
                                      baseRef->id,
                                      baseRef->version.toString(),
                                      "binary"),
                          2);

        ref = pullDependency(*baseRef, this->repo, "develop");
        if (!ref.has_value()) {
            return LINGLONG_ERR("failed to pull base develop " + baseRef->toString(), ref);
        }

        printReplacedText(fmt::format("{:<35}{:<15}{:<15}complete\n",
                                      baseRef->id,
                                      baseRef->version.toString(),
                                      "develop"),
                          2);

        if (runtimeRef) {
            ref = pullDependency(*runtimeRef, this->repo, "binary");
            if (!ref.has_value()) {
                return LINGLONG_ERR("failed to pull runtime binary " + runtimeRef->toString(), ref);
            }

            printReplacedText(fmt::format("{:<35}{:<15}{:<15}complete\n",
                                          runtimeRef->id,
                                          runtimeRef->version.toString(),
                                          "binary"),
                              2);
            ref = pullDependency(*runtimeRef, this->repo, "develop");
            if (!ref.has_value()) {
                return LINGLONG_ERR("failed to pull runtime develop " + runtimeRef->toString(),
                                    ref);
            }

            printReplacedText(fmt::format("{:<35}{:<15}{:<15}complete\n",
                                          runtimeRef->id,
                                          runtimeRef->version.toString(),
                                          "develop"),
                              2);
        }
    }

    auto ret = this->repo.mergeModules();
    if (!ret) {
        return LINGLONG_ERR("failed to merge modules", ret);
    }

    return LINGLONG_OK;
}

std::unique_ptr<utils::OverlayFS> Builder::makeOverlay(
  const std::filesystem::path &lowerdir, const std::filesystem::path &overlayDir) noexcept
{
    std::unique_ptr<utils::OverlayFS> overlay =
      std::make_unique<utils::OverlayFS>(lowerdir,
                                         overlayDir / "upperdir",
                                         overlayDir / "workdir",
                                         overlayDir / "merged");
    if (!overlay->mount()) {
        return nullptr;
    }

    return overlay;
}

// when using overlayfs, remove /etc/localtime and allow the container(box for now) to create the
// correct mount point
void Builder::fixLocaltimeInOverlay(std::unique_ptr<utils::OverlayFS> &baseOverlay)
{
    std::filesystem::path base{ baseOverlay->mergedDirPath() };
    std::vector<std::string> remove{
        "etc/localtime",
        "etc/resolv.conf",
    };

    for (const auto &r : remove) {
        std::error_code ec;
        auto target = base / r;
        if (std::filesystem::exists(target, ec)) {
            std::filesystem::remove(target, ec);
            if (ec) {
                LogW("failed to remove {}: {}", target.c_str(), ec.message().c_str());
            }
        }
    }
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

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    auto fillRes = buildContext.fillContextCfg(cfgBuilder);
    if (!fillRes) {
        return LINGLONG_ERR(fillRes);
    }
    cfgBuilder
      .setAppId(this->project->package.id)
      // overwrite base overlay directory
      .setBasePath(baseOverlay->mergedDirPath(), false)
      .bindDefault()
      .addExtraMount(ocppi::runtime::config::types::Mount{ .destination = "/project",
                                                           .options = { { "rbind", "ro" } },
                                                           .source = this->workingDir,
                                                           .type = "bind" })
      .forwardDefaultEnv()
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1")
      .disableUserNamespace()
      .setCapabilities(privilegeBuilderCaps);

    // overwrite runtime overlay directory
    if (cfgBuilder.getRuntimePath() && runtimeOverlay) {
        cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath(), false);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
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
    QDir(QString::fromStdString(internalDir / "output")).removeRecursively();

    buildOutput = internalDir / "output" / "_build";
    std::error_code ec;
    std::filesystem::create_directories(buildOutput, ec);
    if (ec) {
        return LINGLONG_ERR(
          fmt::format("failed to create directory {}: {}", buildOutput.string(), ec.message())
            .c_str());
    }
    LogD("create develop output success");

    auto res = buildContext.resolve(this->project.value(), buildOutput);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    auto baseLayerPath = buildContext.getBaseLayerPath();
    if (!baseLayerPath) {
        return LINGLONG_ERR(baseLayerPath);
    }
    // prepare overlayfs
    auto overlayDir = internalDir / "overlay";
    baseOverlay = makeOverlay(*baseLayerPath / "files", overlayDir / "build_base");
    if (!baseOverlay) {
        return LINGLONG_ERR("failed to mount build base overlayfs");
    }
    fixLocaltimeInOverlay(baseOverlay);

    if (buildContext.hasRuntime()) {
        auto runtimeLayerPath = buildContext.getRuntimeLayerPath();
        if (!runtimeLayerPath) {
            return LINGLONG_ERR(runtimeLayerPath);
        }
        runtimeOverlay = makeOverlay(*runtimeLayerPath / "files", overlayDir / "build_runtime");
        if (!runtimeOverlay) {
            return LINGLONG_ERR("failed to mount build runtime overlayfs");
        }
    }

    res = processBuildDepends();
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
    QDir appCache(QString::fromStdString(internalDir / "cache"));
    if (!appCache.mkpath(".")) {
        return LINGLONG_ERR("make path " + appCache.absolutePath() + ": failed.");
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    res = buildContext.fillContextCfg(cfgBuilder);
    if (!res) {
        return LINGLONG_ERR(res);
    }
    cfgBuilder.setAppId(this->project->package.id)
      .setBasePath(baseOverlay->mergedDirPath(), false)
      .bindDefault()
      .setStartContainerHooks(
        std::vector<ocppi::runtime::config::types::Hook>{ ocppi::runtime::config::types::Hook{
          .path = "/sbin/ldconfig",
        } })
      .forwardDefaultEnv()
      .addMask({
        "/project/linglong/output",
        "/project/linglong/overlay",
      })
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1")
      .disableUserNamespace()
      .setCapabilities(privilegeBuilderCaps);

    if (this->buildOptions.isolateNetWork) {
        cfgBuilder.isolateNetWork();
    }

    if (cfgBuilder.getRuntimePath() && runtimeOverlay) {
        cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath(), false);
    }

    // write ld.so.conf
    QString ldConfPath = appCache.absoluteFilePath("ld.so.conf");
    std::string triplet = projectRef->arch.getTriplet();
    std::string ldRawConf = cfgBuilder.ldConf(triplet);

    QFile ldsoconf{ ldConfPath };
    if (!ldsoconf.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(ldsoconf);
    }
    ldsoconf.write(ldRawConf.c_str());
    // must be closed here, this conf will be used later.
    ldsoconf.close();

    cfgBuilder.addExtraMounts(std::vector<ocppi::runtime::config::types::Mount>{
      ocppi::runtime::config::types::Mount{ .destination = LINGLONG_BUILDER_HELPER,
                                            .options = { { "rbind", "ro" } },
                                            .source = LINGLONG_BUILDER_HELPER,
                                            .type = "bind" },
      ocppi::runtime::config::types::Mount{ .destination = "/project",
                                            .options = { { "rbind", "rw" } },
                                            .source = this->workingDir,
                                            .type = "bind" },
      ocppi::runtime::config::types::Mount{ .destination =
                                              "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
                                            .options = { { "rbind", "ro" } },
                                            .source = ldConfPath.toStdString(),
                                            .type = "bind" } });

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
    if (!container) {
        return LINGLONG_ERR(container);
    }
    LogD("create container success");

    auto arguments = std::vector<std::string>{};
    for (const auto &arg : args) {
        arguments.push_back(arg.toStdString());
    }

    auto process = ocppi::runtime::config::types::Process{};
    process.args = std::move(arguments);
    process.cwd = "/project";
    process.env = { {
      "PREFIX=" + installPrefix,
      // During the build stage, we use overlayfs where /etc/ld.so.cache is safe
      // to serve as the dynamic library cache. This allows direct invocation of
      // ldconfig without the -C option.
      //
      // Note: LINGLONG_LD_SO_CACHE is retained here solely for backward compatibility.
      "LINGLONG_LD_SO_CACHE=/etc/ld.so.cache",
      "TRIPLET=" + triplet,
    } };
    process.noNewPrivileges = true;
    process.terminal = true;

    printMessage("[Start Build]");
    ocppi::runtime::RunOption opt{};
    auto result = (*container)->run(process, opt);
    if (!result) {
        return LINGLONG_ERR(result);
    }
    LogD("run container success");

    return true;
}

utils::error::Result<void> Builder::buildStagePreCommit() noexcept
{
    LINGLONG_TRACE("build stage pre commit");

    auto &project = *this->project;

    // processing buildext.apt.depends
    auto res = generateDependsScript();
    if (!res) {
        return LINGLONG_ERR("failed to generate depends script", res);
    }
    if (!*res) {
        return LINGLONG_OK;
    }

    takeTerminalForeground();

    // clean prepare overlay
    auto overlayDir = internalDir / "overlay";
    QDir(QString::fromStdString(overlayDir / "prepare_base")).removeRecursively();
    QDir(QString::fromStdString(overlayDir / "prepare_runtime")).removeRecursively();

    // prepare overlay
    auto baseLayerPath = buildContext.getBaseLayerPath();
    if (!baseLayerPath) {
        return LINGLONG_ERR(baseLayerPath);
    }
    baseOverlay = makeOverlay(*baseLayerPath / "files", overlayDir / "prepare_base");
    if (!baseOverlay) {
        return LINGLONG_ERR("failed to mount prepare base overlayfs");
    }
    fixLocaltimeInOverlay(baseOverlay);

    if (buildContext.hasRuntime()) {
        auto runtimeLayerPath = buildContext.getRuntimeLayerPath();
        if (!runtimeLayerPath) {
            return LINGLONG_ERR(runtimeLayerPath);
        }
        runtimeOverlay = makeOverlay(*runtimeLayerPath / "files", overlayDir / "prepare_runtime");
        if (!runtimeOverlay) {
            return LINGLONG_ERR("failed to mount prepare runtime overlayfs");
        }
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    auto fillRes = buildContext.fillContextCfg(cfgBuilder);
    if (!fillRes) {
        return LINGLONG_ERR(fillRes);
    }
    cfgBuilder.setAppId(project.package.id)
      .setBasePath(baseOverlay->mergedDirPath(), false)
      .bindDefault()
      .addExtraMount(ocppi::runtime::config::types::Mount{ .destination = "/project",
                                                           .options = { { "rbind", "rw" } },
                                                           .source = this->workingDir,
                                                           .type = "bind" })
      .forwardDefaultEnv()
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1")
      .disableUserNamespace()
      .setCapabilities(privilegeBuilderCaps);

    if (cfgBuilder.getRuntimePath() && runtimeOverlay) {
        cfgBuilder.setRuntimePath(runtimeOverlay->mergedDirPath(), false);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
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
    LogD("install depends success");

    // merge subdirectory
    // 1. merge base to runtime, Or
    // 2. merge base and runtime to app,
    // base prefix is /usr, and runtime prefix is /runtime
    std::vector<std::filesystem::path> src = { baseOverlay->upperDirPath() / "usr" };
    if (project.package.kind == "app" || project.package.kind == "extension") {
        if (runtimeOverlay) {
            src.push_back(runtimeOverlay->upperDirPath());
        }
    }
    detail::mergeOutput(src, buildOutput, { "bin/", "sbin/", "lib/" });

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateAppConf() noexcept
{
    LINGLONG_TRACE("generate application configure");

    auto &project = *this->project;
    if (project.package.kind != "app") {
        return LINGLONG_OK;
    }

    // generate application's configure file
    auto scriptFile = QString(LINGLONG_LIBEXEC_DIR) + "/app-conf-generator";
    auto useInstalledFile = utils::global::linglongInstalled() && QFile(scriptFile).exists();
    QScopedPointer<QTemporaryDir> dir;
    if (!useInstalledFile) {
        LogD("Dumping app-conf-generator from qrc...");
        dir.reset(new QTemporaryDir);
        // 便于在执行失败时进行调试
        dir->setAutoRemove(false);
        scriptFile = dir->filePath("app-conf-generator");
        QFile::copy(":/scripts/app-conf-generator", scriptFile);
    }
    auto output =
      utils::command::Cmd("bash").exec({ "-e",
                                         scriptFile,
                                         QString::fromStdString(project.package.id),
                                         QString::fromStdString(buildOutput.string()) });
    if (!output) {
        return LINGLONG_ERR(output);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::installFiles() noexcept
{
    LINGLONG_TRACE("install files");

    if (!this->project) {
        return LINGLONG_ERR("not a linyaps project");
    }
    auto &project = *this->project;

    auto appIDPrintWidth = project.package.id.size() + 5;
    printMessage("[Install Files]");
    printMessage(fmt::format("{:<{}}{:<15}{:<15}", "Package", appIDPrintWidth, "Version", "Module"),
                 2);
    // 保存全量的develop, runtime需要对旧的ll-builder保持兼容
    if (this->buildOptions.fullDevelop) {
        utils::copyDirectory(buildOutput, internalDir / "output" / "develop" / "files");
    }

    std::vector<api::types::v1::BuilderProjectModules> projectModules;
    auto hasDevelop = false;
    auto hasBinary = false;
    if (project.modules.has_value()) {
        for (const auto &module : *project.modules) {
            if (module.name == "develop") {
                hasDevelop = true;
            } else if (module.name == "binary") {
                hasBinary = true;
            }
            projectModules.emplace_back(module);
            packageModules.emplace_back(module.name);
        }
    }
    if (!hasDevelop) {
        packageModules.emplace_back("develop");
        // 如果没有develop模块，则添加一个默认的，包含include/**, lib/debug/**, lib/**.a三种匹配
        if (!this->buildOptions.fullDevelop) {
            projectModules.emplace_back(api::types::v1::BuilderProjectModules{
              .files = { "^/include/.+", "^/lib/debug/.+", "^/lib/.+\\.a$" },
              .name = "develop",
            });
        }
    }
    // if binary module doesn't exist, add it as default
    if (!hasBinary) {
        packageModules.emplace_back("binary");
        projectModules.emplace_back(api::types::v1::BuilderProjectModules{
          .files = { "/" },
          .name = "binary",
        });
    }

    for (const auto &module : projectModules) {
        std::unordered_set<std::string> installRules;
        auto moduleDir = internalDir / "output" / module.name;
        auto moduleFilesDir = moduleDir / "files";
        std::error_code ec;
        std::filesystem::create_directories(moduleFilesDir, ec);
        if (ec) {
            return LINGLONG_ERR(
              fmt::format("failed to create module directory {}: {}", moduleFilesDir, ec.message())
                .c_str());
        }
        printReplacedText(fmt::format("{:<{}}{:<15}{:<15}installing",
                                      project.package.id,
                                      appIDPrintWidth,
                                      project.package.version,
                                      module.name),
                          2);
        LogD("install module {}", module.name);
        for (const auto &file : module.files) {
            installRules.emplace(file);
        }
        auto ret = installModule(buildOutput, moduleFilesDir, installRules);
        if (!ret.has_value()) {
            return LINGLONG_ERR("install module", ret);
        }

        // save installed files to ${appid}.install
        const auto installRulePath = moduleDir / (project.package.id + ".install");
        std::ofstream configFile(installRulePath, std::ios::out | std::ios::trunc);
        if (!configFile.is_open()) {
            return LINGLONG_ERR(fmt::format("failed to open file {}", installRulePath).c_str());
        }
        for (const auto &installed : *ret) {
            configFile << (std::filesystem::path{ "/" } / installed).string() << '\n';
        }
        configFile.close();
        if (!configFile) {
            return LINGLONG_ERR(fmt::format("failed to write file {}", installRulePath).c_str());
        }
        printReplacedText(fmt::format("{:<{}}{:<15}{:<15}complete",
                                      project.package.id,
                                      appIDPrintWidth,
                                      project.package.version,
                                      module.name),
                          2);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateEntries() noexcept
{
    LINGLONG_TRACE("generate entries");

    printMessage("");

    auto &project = *this->project;
    if (project.package.kind != "app") {
        return LINGLONG_OK;
    }

    LogD("generate entries");
    // TODO: The current whitelist logic is not very flexible.
    // The application configuration file can be exported after configuring it in the build
    // configuration file(linglong.yaml).
    // 仅导出名单中的目录，以避免意外文件影响系统功能
    const std::filesystem::path exportDirConfigPath = LINGLONG_DATA_DIR "/export-dirs.json";
    if (!std::filesystem::exists(exportDirConfigPath)) {
        return LINGLONG_ERR(
          QString{ "this export config file doesn't exist: %1" }.arg(exportDirConfigPath.c_str()));
    }
    auto exportDirConfig =
      linglong::utils::serialize::LoadJSONFile<linglong::api::types::v1::ExportDirs>(
        exportDirConfigPath);
    if (!exportDirConfig) {
        return LINGLONG_ERR(
          QString{ "failed to load export config file: %1" }.arg(exportDirConfigPath.c_str()));
    }

    QDir binaryFiles(QString::fromStdString(internalDir / "output" / "binary" / "files"));
    QDir binaryEntries(QString::fromStdString(internalDir / "output" / "binary" / "entries"));

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
            utils::copyDirectory(binaryFiles.absoluteFilePath(path).toStdString(),
                                 binaryEntries.filePath("share/metainfo").toStdString());
            continue;
        }

        utils::copyDirectory(binaryFiles.absoluteFilePath(path).toStdString(),
                             binaryEntries.absoluteFilePath(path).toStdString());
    }

    if (binaryFiles.exists("lib/systemd/user")) {
        // 配置放到share/systemd/user或lib/systemd/user对systemd来说基本等价
        // 但玲珑仅将share导出到XDG_DATA_DIR，所以要将lib/systemd/user的内容复制到share/systemd/user
        // 2025-03-24 修订
        // XDG_DATA_DIR的优先级高于/usr/lib/systemd，如果应用意外导出了系统服务文件，例如dbus.service，会导致系统功能异常
        // 现在安装应用时会通过generator脚本将lib/systemd/user下的文件复制到优先级最低的generator.late目录
        // 因此构建时将files/lib/systemd/user的内容复制到entries/lib/systemd/user
        if (!binaryEntries.mkpath("lib/systemd/user")) {
            LogW("mkpath files/lib/systemd/user: failed");
        }

        utils::copyDirectory(binaryFiles.filePath("lib/systemd/user").toStdString(),
                             binaryEntries.absoluteFilePath("lib/systemd/user").toStdString());
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::commitToLocalRepo() noexcept
{
    LINGLONG_TRACE("commit to local repo");

    auto &project = *this->project;
    auto appIDPrintWidth = -project.package.id.size() + -5;

    auto info = api::types::v1::PackageInfoV2{
        .arch = { projectRef->arch.toStdString() },
        .channel = projectRef->channel,
        .command = project.command,
        .description = project.package.description,
        .id = project.package.id,
        .kind = project.package.kind,
        .name = project.package.name,
        .permissions = project.permissions,
        .runtime = {},
        .schemaVersion = PACKAGE_INFO_VERSION,
        .version = project.package.version,
    };

    info.base = project.base;
    if (project.runtime) {
        info.runtime = project.runtime;
    }

    if (info.kind == "extension") {
        info.extImpl =
          api::types::v1::ExtensionImpl{ .env = project.package.env, .libs = project.package.libs };
    }

    // 从本地仓库清理旧的ref
    auto existsModules = this->repo.getModuleList(*projectRef);
    for (const auto &module : existsModules) {
        auto result = this->repo.remove(*projectRef, module);
        if (!result) {
            LogD("remove {} {}", projectRef->toString(), result.error().message());
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
    for (const auto &module : std::as_const(packageModules)) {
        QDir moduleOutput(QString::fromStdString(internalDir / "output" / module));
        info.packageInfoV2Module = module;
        auto ret =
          linglong::utils::calculateDirectorySize(moduleOutput.absolutePath().toStdString());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        info.size = static_cast<int64_t>(*ret);

        QFile infoFile{ moduleOutput.filePath("info.json") };
        if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return LINGLONG_ERR(infoFile);
        }

        infoFile.write(nlohmann::json(info).dump().c_str());
        if (infoFile.error() != QFile::NoError) {
            return LINGLONG_ERR(infoFile);
        }
        infoFile.close();

        LogD("copy linglong.yaml to output");

        std::error_code ec;
        std::filesystem::copy(this->projectYamlFile,
                              moduleOutput.filePath("linglong.yaml").toStdString(),
                              ec);
        if (ec) {
            return LINGLONG_ERR(
              QString("copy linglong.yaml to output failed: %1").arg(ec.message().c_str()));
        }
        LogD("import module to layers");
        printReplacedText(QString("%1%2%3%4")
                            .arg(info.id.c_str(), appIDPrintWidth) // NOLINT
                            .arg(info.version.c_str(), -15)        // NOLINT
                            .arg(module.c_str(), -15)              // NOLINT
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
                            .arg(module.c_str(), -15)              // NOLINT
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
    LINGLONG_TRACE(fmt::format("build project {}", this->workingDir / "linglong.yaml"));

    auto &project = *this->project;
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

    if (!(res = runtimeCheck())) {
        return LINGLONG_ERR("stage runtime check error", res);
    }

    printMessage("Successfully build " + project.package.id);

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportUAB(const ExportOption &option,
                                              const std::filesystem::path &outputFile)
{
    LINGLONG_TRACE("export uab file");

    auto exportWorkingDir = this->workingDir / ".uabBuild";
    package::UABPackager packager{ QDir(QString::fromStdString(workingDir)),
                                   QString::fromStdString(exportWorkingDir) };
    auto exportOpts = option;
    if (exportOpts.compressor.empty()) {
        LogI("Compressor not specified, defaulting to lz4 for UAB export.");
        exportOpts.compressor = "lz4";
    }

    if (exportOpts.modules.empty()) {
        exportOpts.modules.emplace_back("binary");
    }

    const bool distributedOnly = !exportOpts.ref.empty();

    // Try to use uab-header, uab-loader, ll-box and bundling logic from ll-builder-utils if
    // available. Fallback to defaults if ll-builder-utils is not found or fails.
    auto ref = ensureUtils("cn.org.linyaps.builder.utils");
    if (ref) {
        LogD("using cn.org.linyaps.builder.utils");
        std::vector<std::string> args{
            "/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export",
            "--get-header",
            "/project/.uabBuild/uab-header",
            "--get-loader",
            "/project/.uabBuild/uab-loader",
        };

        if (!distributedOnly) {
            args.emplace_back("--get-box");
            args.emplace_back("/project/.uabBuild/ll-box");
        }

        auto res = runFromRepo(*ref, args);
        if (res) {
            std::error_code ec;
            if (std::filesystem::exists(exportWorkingDir / "uab-header", ec)
                && std::filesystem::exists(exportWorkingDir / "uab-loader", ec)) {
                packager.setDefaultHeader(QString::fromStdString(exportWorkingDir / "uab-header"));
                packager.setDefaultLoader(QString::fromStdString(exportWorkingDir / "uab-loader"));
            }

            if (!distributedOnly && std::filesystem::exists(exportWorkingDir / "ll-box", ec)) {
                packager.setDefaultBox(QString::fromStdString(exportWorkingDir / "ll-box"));
            }
        } else {
            LogW("run builder utils error: {}", res.error());
        }

        auto utilsBundler =
          [&ref, &exportOpts, this](const QString &bundleFile,
                                    const QString &bundleDir) -> utils::error::Result<void> {
            LINGLONG_TRACE("use utils to bundle file");

            std::error_code ec;
            const auto relativeBundleFile = QString::fromStdString(
              std::filesystem::relative(bundleFile.toStdString(), workingDir, ec));
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("failed to get relative path {}: {}", bundleFile, ec.message())
                    .c_str());
            }
            const auto relativeBundleDir = QString::fromStdString(
              std::filesystem::relative(bundleDir.toStdString(), workingDir, ec));
            if (ec) {
                return LINGLONG_ERR(
                  fmt::format("failed to get relative path {}: {}", bundleDir, ec.message())
                    .c_str());
            }
            if (relativeBundleFile.startsWith("../") || relativeBundleDir.startsWith("../")) {
                return LINGLONG_ERR("file must be in project directory");
            }
            std::vector<std::string> args{
                "/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export",
                "--packdir",
                QString("%1:%2")
                  .arg(QDir("/project").absoluteFilePath(relativeBundleDir),
                       QDir("/project").absoluteFilePath(relativeBundleFile))
                  .toStdString()
            };

            args.emplace_back("-z");
            args.emplace_back(exportOpts.compressor);
            return runFromRepo(*ref, args);
        };
        packager.setBundleCB(utilsBundler);
    } else {
        LogD("cn.org.linyaps.builder.utils not found, using system tools");
    }

    const bool underProject = this->project.has_value();
    auto curRef = [this,
                   &exportOpts,
                   underProject,
                   distributedOnly]() -> utils::error::Result<package::Reference> {
        LINGLONG_TRACE("get current reference");

        if (distributedOnly) {
            auto fuzzyRef = package::FuzzyReference::parse(exportOpts.ref);
            if (!fuzzyRef) {
                return LINGLONG_ERR("fuzzy ref", fuzzyRef);
            }

            auto targetRef = this->repo.clearReference(*fuzzyRef, { .fallbackToRemote = false });
            if (!targetRef) {
                return LINGLONG_ERR("clear ref", targetRef);
            }

            return targetRef;
        }

        if (!underProject) {
            return LINGLONG_ERR("not under project");
        }
        return currentReference(*this->project);
    }();

    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    QString uabFile;
    if (!outputFile.empty()) {
        if (outputFile.is_absolute()) {
            uabFile = QString::fromStdString(outputFile);
        } else {
            uabFile = QDir::current().absoluteFilePath(QString::fromStdString(outputFile));
        }
    } else {
        uabFile = QString::fromStdString(workingDir / uabExportFilename(*curRef));
    }

    // export single ref
    if (distributedOnly) {
        auto layerDir = this->repo.getLayerDir(*curRef);
        if (!layerDir) {
            return LINGLONG_ERR(layerDir);
        }

        auto ret = packager.appendLayer(*layerDir);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        ret = packager.pack(uabFile, true);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }

        return LINGLONG_OK;
    }

    // if we get there, project must be set
    if (!underProject) {
        return LINGLONG_ERR("project is not set");
    }

    if (!option.iconPath.empty()) {
        if (auto ret = packager.setIcon(QFileInfo{ option.iconPath.c_str() }); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (underProject && this->project->exclude) {
        auto ret = packager.exclude(project->exclude.value());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (underProject && this->project->include) {
        auto ret = packager.include(project->include.value());
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (this->project->runtime) {
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

    auto baseRef = clearDependency(this->project->base, false, false);
    if (!baseRef) {
        return LINGLONG_ERR(baseRef);
    }

    auto baseDir = this->repo.getLayerDir(*baseRef);
    if (!baseDir) {
        return LINGLONG_ERR(baseDir);
    }

    auto ret = packager.appendLayer(*baseDir);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (!option.compressor.empty()) {
        packager.setCompressor(option.compressor.c_str());
    }

    if (this->project->runtime) {
        auto runtimeRef = clearDependency(this->project->runtime.value(), false, false);
        if (!runtimeRef) {
            return LINGLONG_ERR(runtimeRef);
        }

        auto runtimeDir = this->repo.getLayerDir(*runtimeRef);
        if (!runtimeDir) {
            return LINGLONG_ERR(runtimeDir);
        }

        auto ret = packager.appendLayer(*runtimeDir);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
    }

    auto appDir = this->repo.getLayerDir(*curRef);
    if (!appDir) {
        return LINGLONG_ERR(appDir);
    }

    ret = packager.appendLayer(*appDir); // app layer must be the last of appended layer
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (!option.loader.empty()) {
        packager.setLoader(option.loader.c_str());
    }

    if (auto ret = packager.pack(uabFile, false); !ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::exportLayer(const ExportOption &option)
{
    LINGLONG_TRACE("export layer file");

    auto &project = *this->project;
    auto ref = currentReference(project);
    if (!ref) {
        return LINGLONG_ERR(ref);
    }

    auto modules = this->repo.getModuleList(*ref);
    if (modules.empty()) {
        return LINGLONG_ERR(fmt::format("no {} found", ref->toString()));
    }

    package::LayerPackager pkger;
    if (!option.compressor.empty()) {
        pkger.setCompressor(option.compressor.c_str());
    }
    for (const auto &module : modules) {
        if (option.noExportDevelop && module == "develop") {
            continue;
        }

        auto layerDir = this->repo.getLayerDir(*ref, module);
        if (!layerDir) {
            LogE("resolve layer {}/{} failed: {}",
                 ref->toString(),
                 module.c_str(),
                 layerDir.error().message());
            continue;
        }

        auto layerFile = QString::fromStdString(workingDir / layerExportFilename(*ref, module));
        auto ret = pkger.pack(*layerDir, layerFile);
        if (!ret) {
            LogE("export layer {}/{} failed: {}", ref->toString(), module, ret.error());
            return LINGLONG_ERR("export layer {}/{} failed", ret);
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::extractLayer(const QString &layerPath,
                                                 const QString &destination)
{
    LINGLONG_TRACE("extract " + layerPath.toStdString() + " to " + destination.toStdString());

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

    auto output =
      utils::command::Cmd("cp").exec({ "-r", layerDir->absolutePath(), destDir.absolutePath() });
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

    if (!this->project) {
        return LINGLONG_ERR("not under project");
    }

    auto ref = currentReference(*this->project);
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

utils::error::Result<void> Builder::run(std::vector<std::string> modules,
                                        std::vector<std::string> args,
                                        bool debug,
                                        std::vector<std::string> extensions)
{
    LINGLONG_TRACE("run application");

    auto &project = *this->project;
    if (project.package.kind != "app") {
        return LINGLONG_ERR("only app can run");
    }

    auto curRef = currentReference(project);
    if (!curRef) {
        return LINGLONG_ERR(curRef);
    }

    runtime::RunContext runContext(this->repo);
    linglong::runtime::ResolveOptions opts;
    opts.depsBinaryOnly = !debug;
    opts.appModules = std::move(modules);
    if (!extensions.empty()) {
        opts.extensionRefs = extensions;
    }
    auto res = runContext.resolve(*curRef, opts);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    auto *homeEnv = ::getenv("HOME");
    if (homeEnv == nullptr) {
        return LINGLONG_ERR("Couldn't get HOME env.");
    }

    std::vector<ocppi::runtime::config::types::Mount> applicationMounts{};
    if (debug) {
        // 生成 host_gdbinit 可使用 gdb --init-command=linglong/host_gdbinit 从宿主机调试
        {
            auto gdbinit = internalDir / "host_gdbinit";
            auto debugDir = internalDir / "output/develop/files/lib/debug";
            std::ofstream hostConf(gdbinit);
            hostConf << "set substitute-path /project " << workingDir << std::endl;
            hostConf << "set debug-file-directory " << debugDir << std::endl;
            hostConf << "# target remote :12345";
        }
        // 生成 gdbinit 支持在容器中使用gdb $binary调试
        {
            std::string appPrefix = "/opt/apps/" + project.package.id + "/files";
            std::string debugDir = "/usr/lib/debug:/runtime/lib/debug:" + appPrefix + "/lib/debug";
            auto gdbinit = internalDir / "gdbinit";
            std::ofstream f(gdbinit);
            f << "set debug-file-directory " << debugDir << std::endl;

            applicationMounts.push_back(ocppi::runtime::config::types::Mount{
              .destination = std::string{ homeEnv } + "/.gdbinit",
              .options = { { "ro", "rbind" } },
              .source = gdbinit,
              .type = "bind",
            });
        }
    }

    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/project",
      .options = { { "rbind", "rw" } },
      .source = this->workingDir,
      .type = "bind",
    });

    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = LINGLONG_BUILDER_HELPER,
      .options = { { "rbind", "ro" } },
      .source = LINGLONG_BUILDER_HELPER,
      .type = "bind",
    });

    auto appCache = internalDir / "cache";
    std::string ldConfPath = appCache / "ld.so.conf";
    applicationMounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
      .options = { { "rbind", "ro" } },
      .source = ldConfPath,
      .type = "bind",
    });

    uid = getuid();
    gid = getgid();

    {
        // Since ldconfig removes and regenerates the cache file, the cache directory must be
        // writable. Therefore, we must generate the ld cache in a separate running
        linglong::generator::ContainerCfgBuilder cfgBuilder;
        res = runContext.fillContextCfg(cfgBuilder);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        cfgBuilder.setAppId(curRef->id)
          .setAppCache(appCache, false)
          .addUIdMapping(uid, uid, 1)
          .addGIdMapping(gid, gid, 1)
          .bindDefault()
          .addExtraMounts(applicationMounts)
          .enableSelfAdjustingMount()
          .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1");

        // write ld.so.conf
        std::string triplet = curRef->arch.getTriplet();
        std::string ldRawConf = cfgBuilder.ldConf(triplet);

        QFile ldsoconf{ ldConfPath.c_str() };
        if (!ldsoconf.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR(ldsoconf);
        }
        ldsoconf.write(ldRawConf.c_str());
        // must be closed here, this conf will be used later.
        ldsoconf.close();

        if (!cfgBuilder.build()) {
            auto err = cfgBuilder.getError();
            return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
        }

        auto container = this->containerBuilder.create(cfgBuilder);
        if (!container) {
            return LINGLONG_ERR(container);
        }

        ocppi::runtime::config::types::Process process{ .args = std::vector<std::string>{
                                                          "/sbin/ldconfig",
                                                          "-X",
                                                          "-C",
                                                          "/run/linglong/cache/ld.so.cache" } };
        ocppi::runtime::RunOption opt{};
        auto result = (*container)->run(process, opt);
        if (!result) {
            return LINGLONG_ERR("failed to generate ld cache", result);
        }
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;

    cfgBuilder.setAppId(curRef->id)
      .setAppCache(appCache)
      .enableLDCache()
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
      .addExtraMounts(applicationMounts)
      .enableSelfAdjustingMount()
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1");

#ifdef LINGLONG_FONT_CACHE_GENERATOR
    cfgBuilder.enableFontCache()
#endif

      res = runContext.fillContextCfg(cfgBuilder);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);

    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::config::types::Process process;

    if (!args.empty()) {
        process.args = std::move(args);
    } else {
        process.args = project.command;
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

utils::error::Result<void> Builder::runFromRepo(const package::Reference &ref,
                                                const std::vector<std::string> &args)
{
    LINGLONG_TRACE("run with ref " + ref.toString());

    runtime::RunContext runContext(this->repo);
    auto res = runContext.resolve(ref);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    uid = getuid();
    gid = getgid();

    auto appCache = internalDir / "cache" / ref.id;
    std::error_code ec;
    if (!std::filesystem::create_directories(appCache, ec) && ec) {
        return LINGLONG_ERR("failed to create temp cache directory");
    }

    {
        // generate ld cache
        std::string ldConfPath = appCache / "ld.so.conf";

        linglong::generator::ContainerCfgBuilder cfgBuilder;
        res = runContext.fillContextCfg(cfgBuilder);
        if (!res) {
            return LINGLONG_ERR(res);
        }
        cfgBuilder.setAppId(ref.id)
          .setAppCache(appCache, false)
          .addUIdMapping(uid, uid, 1)
          .addGIdMapping(gid, gid, 1)
          .bindDefault()
          .addExtraMount(ocppi::runtime::config::types::Mount{
            .destination = "/etc/ld.so.conf.d/zz_deepin-linglong-app.conf",
            .options = { { "rbind", "ro" } },
            .source = ldConfPath,
            .type = "bind" })
          .enableSelfAdjustingMount()
          .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1");

        // write ld.so.conf
        std::string triplet = ref.arch.getTriplet();
        std::string ldRawConf = cfgBuilder.ldConf(triplet);

        QFile ldsoconf{ ldConfPath.c_str() };
        if (!ldsoconf.open(QIODevice::WriteOnly)) {
            return LINGLONG_ERR(ldsoconf);
        }
        ldsoconf.write(ldRawConf.c_str());
        // must be closed here, this conf will be used later.
        ldsoconf.close();

        if (!cfgBuilder.build()) {
            auto err = cfgBuilder.getError();
            return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
        }

        auto container = this->containerBuilder.create(cfgBuilder);
        if (!container) {
            return LINGLONG_ERR(container);
        }

        ocppi::runtime::config::types::Process process{ .args = std::vector<std::string>{
                                                          "/sbin/ldconfig",
                                                          "-X",
                                                          "-C",
                                                          "/run/linglong/cache/ld.so.cache" } };
        ocppi::runtime::RunOption opt{};
        auto result = (*container)->run(process, opt);
        if (!result) {
            return LINGLONG_ERR("failed to generate ld cache", result);
        }
    }

    linglong::generator::ContainerCfgBuilder cfgBuilder;
    res = runContext.fillContextCfg(cfgBuilder);
    if (!res) {
        return LINGLONG_ERR(res);
    }
    cfgBuilder.setAppId(ref.id)
      .setAppCache(appCache)
      .enableLDCache()
      .bindDefault()
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .addExtraMount(ocppi::runtime::config::types::Mount{ .destination = "/project",
                                                           .options = { { "rbind", "rw" } },
                                                           .source = this->workingDir,
                                                           .type = "bind" })
      .enableSelfAdjustingMount()
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1");

    if (!cfgBuilder.build()) {
        auto err = cfgBuilder.getError();
        return LINGLONG_ERR("build cfg error: " + QString::fromStdString(err.reason));
    }

    auto container = this->containerBuilder.create(cfgBuilder);
    if (!container) {
        return LINGLONG_ERR(container);
    }

    ocppi::runtime::RunOption opt{};
    auto result =
      (*container)->run(ocppi::runtime::config::types::Process{ .args = std::move(args) }, opt);
    if (!result) {
        return LINGLONG_ERR("failed to run with " + ref.toString());
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Builder::runtimeCheck()
{
    LINGLONG_TRACE("runtime check");
    printMessage("[Runtime Check]");
    // skip runtime check for non-app packages
    auto &project = *this->project;
    if (project.package.kind != "app") {
        printMessage("Runtime check skipped", 2);
        return LINGLONG_OK;
    }
    printMessage("Start runtime check", 2);
    // 导出uab时需要使用main-check统计的信息，所以无论是否跳过检查，都需要执行main-check
    auto ret =
      this->run(packageModules,
                { { std::filesystem::path{ LINGLONG_BUILDER_HELPER } / "main-check.sh" } });
    // ignore runtime check if skipCheckOutput is set
    if (this->buildOptions.skipCheckOutput) {
        printMessage("Runtime check ignored", 2);
        return LINGLONG_OK;
    }
    if (!ret) {
        printMessage("Runtime check failed", 2);
        return LINGLONG_ERR(ret);
    }
    printMessage("Runtime check done", 2);
    return LINGLONG_OK;
}

utils::error::Result<void> Builder::generateEntryScript() noexcept
{
    LINGLONG_TRACE("generate entry script");

    auto &project = *this->project;
    QFile entry{ QString::fromStdString(internalDir / "entry.sh") };
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

    LogD("build script: {}", scriptContent);

    if (!entry.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ExeOwner)) {
        return LINGLONG_ERR("set file permission error:", entry);
    }
    LogD("generated entry.sh success");

    return LINGLONG_OK;
}

utils::error::Result<bool> Builder::generateBuildDependsScript() noexcept
{
    LINGLONG_TRACE("generate build depends script");

    auto &project = *this->project;
    if (project.buildext) {
        std::string content;

        auto &buildext = *project.buildext;
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
            auto res = utils::writeFile(internalDir / "buildext.sh", content);

            return res ? utils::error::Result<bool>(true) : LINGLONG_ERR(res);
        }
    }

    return false;
}

utils::error::Result<bool> Builder::generateDependsScript() noexcept
{
    LINGLONG_TRACE("generate depends script");

    auto &project = *this->project;
    if (project.buildext) {
        std::string content;

        auto &buildext = *project.buildext;
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
            auto res = utils::writeFile(internalDir / "buildext.sh", content);

            return res ? utils::error::Result<bool>(true) : LINGLONG_ERR(res);
        }
    }

    return false;
}

void Builder::takeTerminalForeground()
{
    struct sigaction sa{};

    sa.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &sa, NULL);

    int pgid = getpgid(0);
    int tty = open("/dev/tty", O_RDONLY);
    if (tty != -1) {
        if (pgid != -1) {
            tcsetpgrp(tty, pgid);
        }

        if (tcgetpgrp(tty) != pgid) {
            LogW("{} is not terminal foreground process group", pgid);
        }

        close(tty);
    }
}

void Builder::printBasicInfo()
{
    printMessage("[Builder info]");
    printMessage(std::string("Linglong Builder Version: ") + LINGLONG_VERSION, 2);
    printMessage("[Build Target]");
    auto &project = *this->project;
    printMessage(project.package.id, 2);
    printMessage("[Project Info]");
    printMessage("Package Name: " + project.package.name, 2);
    printMessage("Version: " + project.package.version, 2);
    printMessage("Package Type: " + project.package.kind, 2);
    printMessage("Build Arch: " + projectRef->arch.toStdString(), 2);
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

bool Builder::checkDeprecatedInstallFile()
{
    auto installFilepath = this->workingDir / (this->project->package.id + ".install");
    std::error_code ec;
    return !std::filesystem::exists(installFilepath, ec);
}

std::string Builder::uabExportFilename(const linglong::package::Reference &ref)
{
    return fmt::format("{}_{}_{}_{}.uab",
                       ref.id,
                       ref.version.toString(),
                       ref.arch.toStdString(),
                       ref.channel);
}

std::string Builder::layerExportFilename(const linglong::package::Reference &ref,
                                         const std::string &module)
{
    return fmt::format("{}_{}_{}_{}.layer",
                       ref.id,
                       ref.version.toString(),
                       ref.arch.toStdString(),
                       module);
}
} // namespace linglong::builder
