/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/container.h"

#include "configure.h"
#include "linglong/common/dir.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/overlayfs_driver.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/file.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/overlayfs.h"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <fmt/format.h>

#include <cassert>
#include <fstream>
#include <utility>

#include <unistd.h>

namespace {
void mergeProcessConfig(ocppi::runtime::config::types::Process &dst,
                        const ocppi::runtime::config::types::Process &src)
{
    if (src.user) {
        dst.user = src.user;
    }

    if (src.apparmorProfile) {
        dst.apparmorProfile = src.apparmorProfile;
    }

    if (src.args) {
        dst.args = src.args;
    }

    if (src.capabilities) {
        dst.capabilities = src.capabilities;
    }

    if (src.commandLine) {
        dst.commandLine = src.commandLine;
    }

    if (src.consoleSize) {
        dst.consoleSize = src.consoleSize;
    }

    if (!src.cwd.empty()) {
        dst.cwd = src.cwd;
    }

    if (src.env) {
        if (!dst.env) {
            dst.env = src.env;
        } else {
            auto &dstEnv = dst.env.value();
            for (const auto &env : src.env.value()) {
                auto key = env.find_first_of('=');
                if (key == std::string::npos) {
                    continue;
                }

                auto it =
                  std::find_if(dstEnv.begin(), dstEnv.end(), [&key, &env](const std::string &dst) {
                      return dst.rfind(std::string_view(env.data(), key + 1), 0) == 0;
                  });

                if (it != dstEnv.end()) {
                    LogW("environment set multiple times {} {}", *it, env);
                    *it = env;
                } else {
                    dstEnv.emplace_back(env);
                }
            }
        }
    }

    if (src.ioPriority) {
        dst.ioPriority = src.ioPriority;
    }

    if (src.noNewPrivileges) {
        dst.noNewPrivileges = src.noNewPrivileges;
    }

    if (src.oomScoreAdj) {
        dst.oomScoreAdj = src.oomScoreAdj;
    }

    if (src.rlimits) {
        dst.rlimits = src.rlimits;
    }

    if (src.scheduler) {
        dst.scheduler = src.scheduler;
    }

    if (src.selinuxLabel) {
        dst.selinuxLabel = src.selinuxLabel;
    }

    if (src.terminal) {
        dst.terminal = src.terminal;
    }

    if (src.user) {
        dst.user = src.user;
    }
}
} // namespace

namespace linglong::runtime {

auto ContainerContext::create(RunContext &context, CreateOptions options)
  -> utils::error::Result<std::unique_ptr<ContainerContext>>
{
    LINGLONG_TRACE("create container context");

    const auto &containerID = context.getContainerId();
    auto bundleDir = makeBundleDir(containerID, options.bundleSuffix);
    if (!bundleDir) {
        return LINGLONG_ERR("create bundle dir", bundleDir);
    }

    auto res = utils::ensureDirectory(*bundleDir / "rootfs");
    if (!res) {
        return LINGLONG_ERR(res);
    }

    if (options.appCache) {
        res = utils::ensureDirectory(*options.appCache);
        if (!res) {
            return LINGLONG_ERR("ensure app cache directory", res);
        }
    }

    auto containerContext =
      std::unique_ptr<ContainerContext>{ new ContainerContext(std::move(containerID),
                                                              std::move(*bundleDir),
                                                              std::move(options.appCache)) };

    return containerContext;
}

ContainerContext::ContainerContext(std::string containerID,
                                   std::filesystem::path bundleDir,
                                   std::optional<std::filesystem::path> appCache)
    : containerID(std::move(containerID))
    , bundleDir(std::move(bundleDir))
    , appCache(std::move(appCache))
{
}

auto ContainerContext::setupOverlayFS(RunContext &context, bool persistent)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("setup container context overlayfs");

    auto parsedMode = OverlayFSDriver::modeFromString(*context.getConfig().overlayfs);
    if (!parsedMode) {
        return LINGLONG_ERR("invalid overlayfs mode", parsedMode);
    }

    auto driver = OverlayFSDriver::create(*parsedMode);
    if (!this->appCache) {
        return LINGLONG_ERR("app cache is empty");
    }

    const auto &baseLayer = context.getBaseLayer();
    if (!baseLayer) {
        return LINGLONG_ERR("base layer not found");
    }

    auto overlayInternal = *this->appCache / "overlay";
    auto overlayFS = driver->createOverlayFS({ baseLayer->getLayerDir()->filesDirPath() },
                                             overlayInternal,
                                             this->bundleDir,
                                             persistent);
    if (!overlayFS) {
        return LINGLONG_ERR("create overlayfs", overlayFS);
    }

    if (!(*overlayFS)->mount()) {
        return LINGLONG_ERR("mount overlayfs");
    }

    this->overlayFS = std::move(*overlayFS);

    return LINGLONG_OK;
}

auto ContainerContext::getContainerID() const -> const std::string &
{
    return this->containerID;
}

auto ContainerContext::getBundleDir() const -> const std::filesystem::path &
{
    return this->bundleDir;
}

auto ContainerContext::getContainerCache() const -> const std::optional<std::filesystem::path> &
{
    return this->appCache;
}

void ContainerContext::addSecurityContext(std::unique_ptr<SecurityContext> securityContext)
{
    this->securityContexts.emplace_back(std::move(securityContext));
}

auto ContainerContext::genLdConf(const std::string &ldConf, bool overlayEnabled)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("generate ld.so.conf");

    std::filesystem::path ldConfPath;
    if (overlayEnabled) {
        ldConfPath =
          this->bundleDir / "rootfs" / "etc" / "ld.so.conf.d" / "zz_deepin-linglong-app.conf";

        auto ensureRes = utils::ensureDirectory(ldConfPath.parent_path());
        if (!ensureRes) {
            return LINGLONG_ERR("create ld config directory in overlay rootfs", ensureRes);
        }
    } else {
        if (!this->appCache) {
            return LINGLONG_ERR("container cache is empty");
        }

        ldConfPath = *this->appCache / "ld.so.conf";
    }

    auto res = utils::writeFile(ldConfPath, ldConf);
    if (!res) {
        return LINGLONG_ERR(fmt::format("failed to write {}", ldConfPath), res);
    }

    return LINGLONG_OK;
}

ContainerContext::~ContainerContext()
{
    if (this->overlayFS) {
        this->overlayFS->unmount();
    }

    if (getenv("LINGLONG_DEBUG") == nullptr && !this->bundleDir.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(this->bundleDir, ec);
        if (ec) {
            LogW("failed to remove bundle directory {}: {}", this->bundleDir, ec.message());
        }
    }
}

Container::Container(ocppi::runtime::config::types::Config cfg,
                     std::unique_ptr<ContainerContext> context,
                     ocppi::cli::CLI &cli)
    : cfg(std::move(cfg))
    , context(std::move(context))
    , cli(cli)
{
    assert(this->context != nullptr);
    assert(this->cfg.process.has_value());
}

utils::error::Result<void> Container::run(const ocppi::runtime::config::types::Process &process,
                                          ocppi::runtime::RunOption &opt) noexcept
{
    LINGLONG_TRACE(fmt::format("run container {}", this->context->getContainerID()));

    auto curProcess =
      std::move(this->cfg.process).value_or(ocppi::runtime::config::types::Process{});
    mergeProcessConfig(curProcess, process);
    this->cfg.process = std::move(curProcess);

    std::error_code ec;
    if (this->cfg.process->cwd.empty()) {
        auto cwd = std::filesystem::current_path(ec);
        LogD("cwd of process is empty, run process in current directory {}.", cwd);
        this->cfg.process->cwd = std::filesystem::path{ "/run/host/rootfs" } / cwd;
    }

    if (!this->cfg.process->user) {
        this->cfg.process->user =
          ocppi::runtime::config::types::User{ .gid = ::getgid(), .uid = ::getuid() };
    }

    if (isatty(fileno(stdin)) != 0) {
        this->cfg.process->terminal = true;
    }

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/linglong/container-init",
      .options = { { "ro", "rbind" } },
      .source = std::string{ LINGLONG_CONTAINER_INIT },
      .type = "bind",
    });

    auto originalArgs =
      this->cfg.process->args.value_or(std::vector<std::string>{ "echo", "noting to run" });

    auto bundleDir = this->context->getBundleDir();
    auto entrypoint = bundleDir / "entrypoint.sh";
    auto res = utils::writeFile(entrypoint,
                                utils::BashCommandHelper::generateEntrypointScript(originalArgs));
    if (!res) {
        return LINGLONG_ERR(fmt::format("failed to write to {}", entrypoint), res);
    }

    std::filesystem::permissions(entrypoint, std::filesystem::perms::owner_all, ec);
    if (ec) {
        return LINGLONG_ERR("make entrypoint executable", ec);
    }

    auto entrypointPath = "/run/linglong/entrypoint.sh";

    this->cfg.mounts->push_back(ocppi::runtime::config::types::Mount{
      .destination = entrypointPath,
      .options = { { "ro", "rbind" } },
      .source = entrypoint,
      .type = "bind",
    });

    auto cmd = utils::BashCommandHelper::generateExecCommand(entrypointPath);
    this->cfg.process->args = cmd;
    res = utils::writeFile(bundleDir / "config.json", nlohmann::json(this->cfg).dump());
    if (!res) {
        return LINGLONG_ERR("failed to write to config.json", res);
    }

    LogD("run container with bundle {}", bundleDir);
    // 禁用crun自己创建cgroup，便于AM识别和管理玲珑应用
    opt.GlobalOption::extra.emplace_back("--cgroup-manager=disabled");

    auto result = this->cli.run(this->context->getContainerID(), bundleDir, opt);
    if (!result) {
        return LINGLONG_ERR("cli run", result.error());
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
