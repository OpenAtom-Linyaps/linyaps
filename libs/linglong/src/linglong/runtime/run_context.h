/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/api/types/v1/ContainerProcessStateInfo.hpp"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <filesystem>

namespace linglong::runtime {

class RunContext;

class RuntimeLayer
{
public:
    RuntimeLayer(package::Reference ref, RunContext &context);
    ~RuntimeLayer();

    utils::error::Result<void> resolveLayer(
      const QStringList &modules = {}, const std::optional<std::string> &subRef = std::nullopt);

    utils::error::Result<api::types::v1::RepositoryCacheLayersItem> getCachedItem();

    const package::Reference &getReference() const { return reference; }

    const std::optional<package::LayerDir> &getLayerDir() const { return layerDir; }

private:
    package::Reference reference;
    std::reference_wrapper<RunContext> runContext;
    std::optional<package::LayerDir> layerDir;
    std::optional<api::types::v1::RepositoryCacheLayersItem> cachedItem;
    bool temporary;
};

class RunContext
{
public:
    RunContext(repo::OSTreeRepo &r)
        : repo(r)
    {
    }

    ~RunContext();

    utils::error::Result<void> resolve(const linglong::package::Reference &runnable,
                                       bool depsBinaryOnly = false,
                                       const QStringList &appModules = {});
    utils::error::Result<void> resolve(const api::types::v1::BuilderProject &target,
                                       std::filesystem::path buildOutput);

    utils::error::Result<void> fillContextCfg(generator::ContainerCfgBuilder &builder);
    api::types::v1::ContainerProcessStateInfo stateInfo();

    repo::OSTreeRepo &getRepo() const { return repo; }

    const std::string &getContainerId() const { return containerID; }

    const std::optional<RuntimeLayer> &getBaseLayer() const { return baseLayer; }

    const std::optional<RuntimeLayer> &getRuntimeLayer() const { return runtimeLayer; }

    utils::error::Result<std::filesystem::path> getBaseLayerPath() const;
    utils::error::Result<std::filesystem::path> getRuntimeLayerPath() const;

    utils::error::Result<api::types::v1::RepositoryCacheLayersItem> getCachedAppItem();

    bool hasRuntime() const { return !!runtimeLayer; }

private:
    utils::error::Result<void> resolveLayer(bool depsBinaryOnly, const QStringList &appModules);
    utils::error::Result<void> resolveExtension(RuntimeLayer &layer);
    utils::error::Result<void> fillExtraAppMounts(generator::ContainerCfgBuilder &builder);

    repo::OSTreeRepo &repo;

    std::optional<RuntimeLayer> baseLayer;
    std::optional<RuntimeLayer> runtimeLayer;
    std::optional<RuntimeLayer> appLayer;
    std::vector<RuntimeLayer> extensionLayers;

    std::string targetId;
    std::optional<std::filesystem::path> appOutput;
    std::optional<std::filesystem::path> runtimeOutput;
    std::optional<std::filesystem::path> extensionOutput;

    std::string containerID;
    std::filesystem::path bundle;
};

} // namespace linglong::runtime
