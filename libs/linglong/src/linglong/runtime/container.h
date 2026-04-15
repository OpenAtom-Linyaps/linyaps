/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/runtime/security_context.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/CLI.hpp"
#include "ocppi/runtime/config/types/Config.hpp"
#include "ocppi/runtime/config/types/Process.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace linglong::utils {
class OverlayFS;
}

namespace linglong::runtime {

class RunContext;

class ContainerContext
{
public:
    struct CreateOptions
    {
        std::string bundleSuffix;
        std::optional<std::filesystem::path> appCache;
    };

    static auto create(RunContext &context, CreateOptions options)
      -> utils::error::Result<std::unique_ptr<ContainerContext>>;
    ~ContainerContext();
    ContainerContext(std::string containerID,
                     std::filesystem::path bundleDir,
                     std::optional<std::filesystem::path> appCache = std::nullopt);

    ContainerContext(const ContainerContext &) = delete;
    auto operator=(const ContainerContext &) -> ContainerContext & = delete;
    ContainerContext(ContainerContext &&) = delete;
    auto operator=(ContainerContext &&) -> ContainerContext & = delete;

    [[nodiscard]] auto getContainerID() const -> const std::string &;
    [[nodiscard]] auto getBundleDir() const -> const std::filesystem::path &;
    [[nodiscard]] auto getContainerCache() const -> const std::optional<std::filesystem::path> &;
    void addSecurityContext(std::unique_ptr<SecurityContext> securityContext);
    auto setupOverlayFS(RunContext &context, bool readOnly) -> utils::error::Result<void>;
    auto genLdConf(const std::string &ldConf, bool overlayEnabled) -> utils::error::Result<void>;

private:
    std::string containerID;
    std::filesystem::path bundleDir;
    std::optional<std::filesystem::path> appCache;
    std::vector<std::unique_ptr<SecurityContext>> securityContexts;
    std::unique_ptr<utils::OverlayFS> overlayFS;
};

class Container
{
public:
    Container(ocppi::runtime::config::types::Config cfg,
              std::unique_ptr<ContainerContext> context,
              ocppi::cli::CLI &cli);

    utils::error::Result<void> run(const ocppi::runtime::config::types::Process &process,
                                   ocppi::runtime::RunOption &opt) noexcept;

private:
    ocppi::runtime::config::types::Config cfg;
    std::unique_ptr<ContainerContext> context;
    ocppi::cli::CLI &cli;
};

}; // namespace linglong::runtime
