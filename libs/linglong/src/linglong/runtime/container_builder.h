/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/RunContextConfig.hpp"
#include "linglong/api/types/v1/RuntimeConfigure.hpp"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/runtime/container.h"
#include "linglong/runtime/security_context.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"

#include <QCryptographicHash>

#include <filesystem>
#include <functional>
#include <map>
#include <optional>

namespace linglong::cli {
struct RunOptions;
}

namespace linglong::runtime {

class RunContext;

// Used to obtain a clean container bundle directory.
utils::error::Result<std::filesystem::path> makeBundleDir(const std::string &containerID,
                                                          const std::string &bundleSuffix = "");

std::string genContainerID(const api::types::v1::RunContextConfig &config) noexcept;

struct CommonContainerOptions
{
    std::optional<std::filesystem::path> containerCachePath;
    std::vector<ocppi::runtime::config::types::Mount> extraMounts;
};

struct RunContainerOptions
{
    auto applyRuntimeConfig(const api::types::v1::RuntimeConfigure &runtimeConfig) noexcept
      -> utils::error::Result<void>;
    auto applyCliRunOptions(const cli::RunOptions &options) noexcept -> utils::error::Result<void>;
    void enableSecurityContext(const std::vector<SecurityContextType> &ctxs);
    [[nodiscard]] auto getEnv() const noexcept -> const std::map<std::string, std::string> &;
    [[nodiscard]] auto getCapabilities() const noexcept -> const std::vector<std::string> &;
    [[nodiscard]] auto getSecurityContexts() const noexcept
      -> const std::vector<SecurityContextType> &;
    [[nodiscard]] auto isPrivileged() const noexcept -> bool;

    CommonContainerOptions common;

    bool privileged{ false };
    std::map<std::string, std::string> env;
    std::vector<std::string> capabilities;
    std::vector<SecurityContextType> securityContexts;
};

struct BuilderContainerOptions
{
    CommonContainerOptions common;
    // overwrite base path and runtime path
    std::filesystem::path basePath;
    std::optional<std::filesystem::path> runtimePath;
    bool isolateNetWork{ false };
    std::vector<std::string> masks;
    std::vector<ocppi::runtime::config::types::Hook> startContainerHooks;
};

class ContainerBuilder
{
public:
    explicit ContainerBuilder(ocppi::cli::CLI &cli);

    auto createBuildContainer(runtime::RunContext &context,
                              const BuilderContainerOptions &options) noexcept
      -> utils::error::Result<std::unique_ptr<Container>>;

    auto createInitContainer(runtime::RunContext &context,
                             const CommonContainerOptions &options = {}) noexcept
      -> utils::error::Result<std::unique_ptr<Container>>;

    auto createRunContainer(runtime::RunContext &context,
                            const RunContainerOptions &options) noexcept
      -> utils::error::Result<std::unique_ptr<Container>>;

private:
    enum class ContainerMode {
        Init,
        Build,
        Run,
    };

    struct PreparedContainer
    {
        generator::ContainerCfgBuilder cfgBuilder;
        std::reference_wrapper<runtime::RunContext> runContext;
        std::unique_ptr<ContainerContext> context;
        ContainerMode mode{ ContainerMode::Run };
    };

    auto create(const linglong::generator::ContainerCfgBuilder &cfgBuilder,
                std::unique_ptr<ContainerContext> context) noexcept
      -> utils::error::Result<std::unique_ptr<Container>>;
    auto prepareContainer(runtime::RunContext &context,
                          ContainerMode mode,
                          const CommonContainerOptions &options = {}) noexcept
      -> utils::error::Result<PreparedContainer>;
    static auto bundleSuffixFor(ContainerMode mode) -> std::string;
    static auto overlayReadOnlyFor(ContainerMode mode) -> bool;
    static auto appCacheReadOnlyFor(ContainerMode mode) -> bool;
    auto configureInitContainer(PreparedContainer &prepared) noexcept -> utils::error::Result<void>;
    auto finalizeContainer(PreparedContainer &prepared) noexcept
      -> utils::error::Result<std::unique_ptr<Container>>;
    auto configureBuildContainer(PreparedContainer &prepared,
                                 const BuilderContainerOptions &options) noexcept
      -> utils::error::Result<void>;
    auto configureRunContainer(PreparedContainer &prepared,
                               const RunContainerOptions &options) noexcept
      -> utils::error::Result<void>;
    auto normalizeContainerRootfs(const std::filesystem::path &rootfs,
                                  const api::types::v1::RunContextConfig &config) noexcept
      -> utils::error::Result<void>;

    ocppi::cli::CLI &cli;
};

} // namespace linglong::runtime
