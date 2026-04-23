/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container_builder.h"

#include "linglong/cli/cli.h"
#include "linglong/common/dir.h"
#include "linglong/common/strings.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/package/architecture.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <fstream>

#include <unistd.h>

namespace linglong::runtime {

namespace {

const std::vector<std::string> buildContainerCaps = {
    "CAP_CHOWN",   "CAP_DAC_OVERRIDE",     "CAP_FOWNER",     "CAP_FSETID",
    "CAP_KILL",    "CAP_NET_BIND_SERVICE", "CAP_SETFCAP",    "CAP_SETGID",
    "CAP_SETPCAP", "CAP_SETUID",           "CAP_SYS_CHROOT",
};

} // namespace

utils::error::Result<std::filesystem::path> makeBundleDir(const std::string &containerID,
                                                          const std::string &bundleSuffix)
{
    LINGLONG_TRACE("get bundle dir");
    auto bundle = common::dir::getBundleDir(containerID);
    if (!bundleSuffix.empty()) {
        bundle += bundleSuffix;
    }
    std::error_code ec;
    if (std::filesystem::exists(bundle, ec)) {
        std::filesystem::remove_all(bundle, ec);
        if (ec) {
            LogW("failed to remove bundle directory {}: {}", bundle.c_str(), ec.message());
        }
    }

    if (!std::filesystem::create_directories(bundle, ec) && ec) {
        return LINGLONG_ERR(fmt::format("failed to create bundle directory {}", bundle), ec);
    }

    return bundle;
}

std::string genContainerID(const api::types::v1::RunContextConfig &config) noexcept
{
    auto jsonStr = nlohmann::json(config).dump();
    return QCryptographicHash::hash(QByteArray::fromStdString(jsonStr), QCryptographicHash::Sha256)
      .toHex()
      .toStdString();
}

auto RunContainerOptions::applyRuntimeConfig(
  const api::types::v1::RuntimeConfigure &runtimeConfig) noexcept -> utils::error::Result<void>
{
    if (runtimeConfig.env) {
        for (const auto &[key, value] : *runtimeConfig.env) {
            this->env.insert_or_assign(key, value);
        }
    }

    return LINGLONG_OK;
}

auto RunContainerOptions::applyCliRunOptions(const cli::RunOptions &options) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("apply cli run options to run config");

    for (const auto &item : options.envs) {
        auto split = item.find('=');
        if (split == std::string::npos || split == 0) {
            return LINGLONG_ERR(fmt::format("invalid environment variable: {}", item));
        }

        this->env.insert_or_assign(item.substr(0, split), item.substr(split + 1));
    }

    this->privileged = options.privileged;
    this->capabilities.insert(this->capabilities.end(),
                              options.capsAdd.begin(),
                              options.capsAdd.end());

    return LINGLONG_OK;
}

auto RunContainerOptions::getEnv() const noexcept -> const std::map<std::string, std::string> &
{
    return this->env;
}

auto RunContainerOptions::getCapabilities() const noexcept -> const std::vector<std::string> &
{
    return this->capabilities;
}

void RunContainerOptions::enableSecurityContext(const std::vector<SecurityContextType> &ctxs)
{
    for (const auto &type : ctxs) {
        if (std::find(this->securityContexts.begin(), this->securityContexts.end(), type)
            == this->securityContexts.end()) {
            this->securityContexts.emplace_back(type);
        }
    }
}

auto RunContainerOptions::getSecurityContexts() const noexcept
  -> const std::vector<SecurityContextType> &
{
    return this->securityContexts;
}

auto RunContainerOptions::isPrivileged() const noexcept -> bool
{
    return this->privileged;
}

ContainerBuilder::ContainerBuilder(ocppi::cli::CLI &cli)
    : cli(cli)
{
}

auto ContainerBuilder::bundleSuffixFor(ContainerMode mode) -> std::string
{
    switch (mode) {
    case ContainerMode::Init:
        return ".init";
    case ContainerMode::Build:
        return ".build";
    case ContainerMode::Run:
        return "";
    }

    return "";
}

auto ContainerBuilder::overlayReadOnlyFor(ContainerMode mode) -> bool
{
    switch (mode) {
    case ContainerMode::Init:
        return false;
    case ContainerMode::Build:
        return false;
    case ContainerMode::Run:
        return true;
    }

    return true;
}

auto ContainerBuilder::appCacheReadOnlyFor(ContainerMode mode) -> bool
{
    switch (mode) {
    case ContainerMode::Init:
        return false;
    case ContainerMode::Build:
        return false;
    case ContainerMode::Run:
        return true;
    }

    return true;
}

auto ContainerBuilder::createBuildContainer(runtime::RunContext &context,
                                            const BuilderContainerOptions &options) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create build container");

    auto prepared = this->prepareContainer(context, ContainerMode::Build, options.common);
    if (!prepared) {
        return LINGLONG_ERR(prepared);
    }

    auto res = this->configureBuildContainer(*prepared, options);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return this->finalizeContainer(*prepared);
}

auto ContainerBuilder::create(const linglong::generator::ContainerCfgBuilder &cfgBuilder,
                              std::unique_ptr<ContainerContext> context) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create container");

    auto config = cfgBuilder.getConfig();
    if (!context) {
        return LINGLONG_ERR("container context is null");
    }

    return std::make_unique<Container>(config, std::move(context), this->cli);
}

auto ContainerBuilder::prepareContainer(runtime::RunContext &context,
                                        ContainerMode mode,
                                        const CommonContainerOptions &options) noexcept
  -> utils::error::Result<PreparedContainer>
{
    LINGLONG_TRACE("prepare container");

    auto containerContext = ContainerContext::create(context,
                                                     ContainerContext::CreateOptions{
                                                       .bundleSuffix = bundleSuffixFor(mode),
                                                       .appCache = options.containerCachePath,
                                                     });
    if (!containerContext) {
        return LINGLONG_ERR("failed to create container context of " + context.getContainerId(),
                            containerContext);
    }

    PreparedContainer prepared{
        .runContext = context,
        .context = std::move(*containerContext),
        .mode = mode,
    };

    auto res = context.fillContextCfg(prepared.cfgBuilder, prepared.context->getBundleDir());
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (!options.extraMounts.empty()) {
        prepared.cfgBuilder.addExtraMounts(options.extraMounts);
    }

    auto uid = getuid();
    auto gid = getgid();

    prepared.cfgBuilder.setAppId(prepared.runContext.get().getTargetID())
      .setBundlePath(prepared.context->getBundleDir())
      .addUIdMapping(uid, uid, 1)
      .addGIdMapping(gid, gid, 1)
      .bindUserGroup()
      .bindDefault()
      .bindCgroup();

    if (const auto &appCache = prepared.context->getContainerCache(); appCache) {
        prepared.cfgBuilder.setAppCache(*appCache, appCacheReadOnlyFor(mode));
    }

    return prepared;
}

auto ContainerBuilder::finalizeContainer(PreparedContainer &prepared) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("finalize container");

    auto buildErr = prepared.cfgBuilder.build();
    if (!buildErr) {
        return LINGLONG_ERR("build cfg error", buildErr);
    }

    // must be set after cfgBuilder.build()
    std::string triplet;
    bool useOverlayMode = true;
    bool needGenLdConf = false;
    if (prepared.mode == ContainerMode::Init) {
        auto &runContext = prepared.runContext.get();
        const auto &appLayer = runContext.getAppLayer();
        if (!appLayer) {
            return LINGLONG_ERR("app layer not found");
        }

        triplet = appLayer->getReference().arch.getTriplet();
        needGenLdConf = true;
    } else if (prepared.mode == ContainerMode::Build) {
        triplet = package::Architecture::currentCPUArchitecture().getTriplet();
        useOverlayMode = false;
        needGenLdConf = true;
    }

    if (needGenLdConf) {
        auto genRes =
          prepared.context->genLdConf(prepared.cfgBuilder.ldConf(triplet), useOverlayMode);
        if (!genRes) {
            return LINGLONG_ERR("generate ld config", genRes);
        }
    }

    return this->create(prepared.cfgBuilder, std::move(prepared.context));
}

auto ContainerBuilder::configureBuildContainer(PreparedContainer &prepared,
                                               const BuilderContainerOptions &options) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("configure build container");

    prepared.cfgBuilder.setBasePath(options.basePath, false)
      .forwardDefaultEnv()
      .appendEnv("LINYAPS_INIT_SINGLE_MODE", "1")
      .disableUserNamespace()
      .setCapabilities(buildContainerCaps)
      .enableLDConf();

    if (options.runtimePath) {
        prepared.cfgBuilder.setRuntimePath(*options.runtimePath, false);
    }

    if (!options.startContainerHooks.empty()) {
        prepared.cfgBuilder.setStartContainerHooks(options.startContainerHooks);
    }

    if (!options.masks.empty()) {
        prepared.cfgBuilder.addMask(options.masks);
    }

    if (options.isolateNetWork) {
        prepared.cfgBuilder.isolateNetWork();
    }

    auto &runContext = prepared.runContext.get();
    auto res = normalizeContainerRootfs(options.basePath, runContext.getConfig());
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return LINGLONG_OK;
}

auto ContainerBuilder::normalizeContainerRootfs(
  const std::filesystem::path &rootfs, const api::types::v1::RunContextConfig &config) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("normalize container rootfs");

    // these files may be created as symlink, remove them from rootfs to avoid create failure
    std::vector<std::filesystem::path> remove{
        "/etc/localtime",
        "/etc/resolv.conf",
    };

    for (const auto &r : remove) {
        std::error_code ec;
        auto target = rootfs / (r.is_absolute() ? r.relative_path() : r);
        if (std::filesystem::exists(target, ec)) {
            std::filesystem::remove(target, ec);
            if (ec) {
                LogW("failed to remove {}: {}", target, ec.message());
            }
        }
    }

    // create symlink for timezone
    if (config.timezone && !config.timezone->empty()) {
        auto localtimePath = rootfs / "etc/localtime";
        auto timezonePath = generator::ContainerCfgBuilder::zoneinfoMountPoint / *config.timezone;
        std::error_code ec;
        std::filesystem::create_symlink(timezonePath, localtimePath, ec);
        if (ec) {
            return LINGLONG_ERR(
              fmt::format("failed to create symlink {} -> {}", localtimePath, timezonePath),
              ec);
        }
    }

    return LINGLONG_OK;
}

auto ContainerBuilder::configureInitContainer(PreparedContainer &prepared) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("configure init container");

    auto &runContext = prepared.runContext.get();

    prepared.cfgBuilder.bindXDGRuntime().bindHostRoot().bindHostStatics().forwardDefaultEnv();

    if (runContext.getConfig().overlayfs) {
        auto res = prepared.context->setupOverlayFS(runContext, true);
        if (!res) {
            return LINGLONG_ERR("setup overlayfs", res);
        }

        res = normalizeContainerRootfs(prepared.context->getBundleDir() / "rootfs",
                                       runContext.getConfig());
        if (!res) {
            return LINGLONG_ERR(res);
        }

        prepared.cfgBuilder.enableOverlayMode(prepared.context->getBundleDir() / "rootfs", false);
    }

    auto applyRes = runContext.setupCDIDevices(prepared.cfgBuilder);
    if (!applyRes) {
        return LINGLONG_ERR(applyRes);
    }

    return LINGLONG_OK;
}

auto ContainerBuilder::createInitContainer(runtime::RunContext &context,
                                           const CommonContainerOptions &options) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create init container");

    auto prepared = this->prepareContainer(context, ContainerMode::Init, options);
    if (!prepared) {
        return LINGLONG_ERR(prepared);
    }

    auto res = this->configureInitContainer(*prepared);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return this->finalizeContainer(*prepared);
}

auto ContainerBuilder::configureRunContainer(PreparedContainer &prepared,
                                             const RunContainerOptions &options) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("configure run container");

    auto *homeEnv = ::getenv("HOME");
    if (homeEnv == nullptr) {
        return LINGLONG_ERR("HOME is not set");
    }
    std::filesystem::path homePath{ homeEnv };

    prepared.cfgBuilder.setAnnotation(generator::ANNOTATION::LAST_PID, std::to_string(::getpid()))
      .bindDevNode()
      .bindXDGRuntime()
      .bindRemovableStorageMounts()
      .bindHostRoot()
      .bindHostStatics()
      .bindHome(homePath)
      .enablePrivateDir()
      .mapPrivate((homePath / ".ssh").string(), true)
      .mapPrivate((homePath / ".gnupg").string(), true)
      .bindIPC()
      .forwardDefaultEnv();

    std::error_code ec;
    auto socketDir = prepared.context->getBundleDir() / "init";
    std::filesystem::create_directories(socketDir, ec);
    if (ec) {
        return LINGLONG_ERR("failed to create init socket directory", ec);
    }

    prepared.cfgBuilder.addExtraMount(
      ocppi::runtime::config::types::Mount{ .destination = "/run/linglong/init",
                                            .options = std::vector<std::string>{ "bind" },
                                            .source = socketDir.string(),
                                            .type = "bind" });

    if (!options.getEnv().empty()) {
        prepared.cfgBuilder.appendEnv(options.getEnv());
    }

    std::vector<std::string> capabilities;
    if (options.isPrivileged()) {
        if (getuid() != 0) {
            return LINGLONG_ERR("privileged mode requires running as root");
        }

        prepared.cfgBuilder.disableUserNamespace();
        capabilities = { "CAP_CHOWN",    "CAP_DAC_OVERRIDE",     "CAP_FOWNER",     "CAP_FSETID",
                         "CAP_KILL",     "CAP_NET_BIND_SERVICE", "CAP_SETFCAP",    "CAP_SETGID",
                         "CAP_SETPCAP",  "CAP_SETUID",           "CAP_SYS_CHROOT", "CAP_NET_RAW",
                         "CAP_NET_ADMIN" };
    }

    const auto &extraCapabilities = options.getCapabilities();
    capabilities.insert(capabilities.end(), extraCapabilities.begin(), extraCapabilities.end());
    prepared.cfgBuilder.setCapabilities(std::move(capabilities));

    auto &runContext = prepared.runContext.get();
    for (const auto &type : options.getSecurityContexts()) {
        auto manager = getSecurityContextManager(type);
        if (!manager) {
            auto msg = "failed to get security context manager: " + fromType(type);
            return LINGLONG_ERR(msg.c_str());
        }

        auto securityContext = manager->createSecurityContext(runContext, *prepared.context);
        if (!securityContext) {
            auto msg = "failed to create security context: " + fromType(type);
            return LINGLONG_ERR(msg.c_str());
        }

        auto res = (*securityContext)->apply(prepared.cfgBuilder);
        if (!res) {
            auto msg = "failed to apply security context: " + fromType(type);
            return LINGLONG_ERR(msg.c_str(), res);
        }

        prepared.context->addSecurityContext(std::move(*securityContext));
    }

    if (runContext.getConfig().overlayfs) {
        auto res = prepared.context->setupOverlayFS(runContext, false);
        if (!res) {
            return LINGLONG_ERR("setup overlayfs", res);
        }
        prepared.cfgBuilder.enableOverlayMode(prepared.context->getBundleDir() / "rootfs", true);
    }

    auto applyRes = runContext.setupCDIDevices(prepared.cfgBuilder, false);
    if (!applyRes) {
        return LINGLONG_ERR(applyRes);
    }

    return LINGLONG_OK;
}

auto ContainerBuilder::createRunContainer(runtime::RunContext &context,
                                          const RunContainerOptions &options) noexcept
  -> utils::error::Result<std::unique_ptr<Container>>
{
    LINGLONG_TRACE("create run container");

    auto prepared = this->prepareContainer(context, ContainerMode::Run, options.common);
    if (!prepared) {
        return LINGLONG_ERR(prepared);
    }

    auto res = this->configureRunContainer(*prepared, options);
    if (!res) {
        return LINGLONG_ERR(res);
    }

    return this->finalizeContainer(*prepared);
}

} // namespace linglong::runtime
