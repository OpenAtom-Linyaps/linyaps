/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/api/types/v1/ExtensionDefine.hpp"
#include "linglong/api/types/v1/RunContextConfig.hpp"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/layer.h"
#include "linglong/utils/error/error.h"
#include "ocppi/runtime/config/types/Generators.hpp"

#include <filesystem>

namespace linglong::generator {
class ContainerCfgBuilder;
}

namespace linglong::runtime {

struct ResolveOptions
{
    bool depsBinaryOnly{ false };
    std::optional<std::vector<std::string>> appModules;
    std::optional<std::string> baseRef;
    std::optional<std::vector<api::types::v1::CdiDeviceEntry>> cdiDevices;
    std::optional<std::string> runtimeRef;
    std::optional<std::vector<std::string>> extensionRefs;
    std::optional<std::map<std::string, std::vector<api::types::v1::ExtensionDefine>>>
      externalExtensionDefs;
};

class RunContext
{
public:
    explicit RunContext(repo::OSTreeRepo &r)
        : repo(r)
    {
    }

    ~RunContext();

    utils::error::Result<void> resolve(const linglong::package::Reference &runnable,
                                       const ResolveOptions &opts = ResolveOptions{});

    utils::error::Result<void> resolve(const api::types::v1::BuilderProject &target,
                                       const std::filesystem::path &buildOutput);

    utils::error::Result<void> resolve(const api::types::v1::RunContextConfig &config);

    [[nodiscard]] const api::types::v1::RunContextConfig &getConfig() const { return contextCfg; }

    utils::error::Result<void> fillContextCfg(generator::ContainerCfgBuilder &builder,
                                              const std::filesystem::path &bundlePath);
    utils::error::Result<void> setupCDIDevices(generator::ContainerCfgBuilder &builder,
                                               bool applyHooks = true) const;
    api::types::v1::ContainerProcessStateInfo stateInfo();

    [[nodiscard]] repo::OSTreeRepo &getRepo() const noexcept { return repo; }

    [[nodiscard]] const std::string &getContainerId() const noexcept { return containerID; }

    [[nodiscard]] const std::string &getTargetID() const noexcept { return targetId; }

    [[nodiscard]] const std::optional<RuntimeLayer> &getBaseLayer() const noexcept
    {
        return baseLayer;
    }

    [[nodiscard]] const std::optional<RuntimeLayer> &getRuntimeLayer() const noexcept
    {
        return runtimeLayer;
    }

    [[nodiscard]] const std::optional<RuntimeLayer> &getAppLayer() const noexcept
    {
        return appLayer;
    }

    [[nodiscard]] utils::error::Result<std::filesystem::path> getBaseLayerPath() const;
    [[nodiscard]] utils::error::Result<std::filesystem::path> getRuntimeLayerPath() const;

    utils::error::Result<api::types::v1::RepositoryCacheLayersItem> getCachedAppItem();

    [[nodiscard]] bool hasRuntime() const noexcept { return !!runtimeLayer; }

private:
    utils::error::Result<void> resolveLayer(bool depsBinaryOnly,
                                            const std::vector<std::string> &appModules);
    utils::error::Result<void> resolveLayerExtensions(
      RuntimeLayer &layer,
      const std::vector<api::types::v1::ExtensionDefine> &externalExtensionDefs);
    utils::error::Result<void>
    resolveExtension(RuntimeLayer &targetLayer,
                     const std::vector<api::types::v1::ExtensionDefine> &extDefs,
                     std::optional<std::string> channel = std::nullopt,
                     bool skipOnNotFound = false);
    utils::error::Result<void> fillExtraAppMounts(generator::ContainerCfgBuilder &builder,
                                                  const std::filesystem::path &bundlePath);
    void detectDisplaySystem(generator::ContainerCfgBuilder &builder) noexcept;
    utils::error::Result<std::vector<api::types::v1::ExtensionDefine>>
    makeManualExtensionDefine(const std::vector<std::string> &refs);
    std::vector<api::types::v1::ExtensionDefine> matchedExtensionDefines(
      const package::Reference &ref,
      const std::optional<std::map<std::string, std::vector<api::types::v1::ExtensionDefine>>>
        &externalExtensionDefs);
    utils::error::Result<void>
    resolveOverlayMode(std::optional<std::string> requestedMode = std::nullopt);
    utils::error::Result<void> resolveTimeZone();

    repo::OSTreeRepo &repo;
    std::optional<RuntimeLayer> baseLayer;
    std::optional<RuntimeLayer> runtimeLayer;
    std::optional<RuntimeLayer> appLayer;
    std::list<RuntimeLayer> extensionLayers;

    std::string targetId;
    std::optional<std::filesystem::path> appOutput;
    std::optional<std::filesystem::path> runtimeOutput;
    std::optional<std::filesystem::path> extensionOutput;

    std::string containerID;
    std::map<std::string, std::string> environment;

    api::types::v1::RunContextConfig contextCfg;
};

} // namespace linglong::runtime
