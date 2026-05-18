// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/utils/error/error.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace linglong::runtime {

class RunContext;

class RuntimeLayer
{
public:
    RuntimeLayer(const RuntimeLayer &) = delete;
    RuntimeLayer &operator=(const RuntimeLayer &) = delete;
    RuntimeLayer(RuntimeLayer &&) = default;
    RuntimeLayer &operator=(RuntimeLayer &&) = default;

    static utils::error::Result<RuntimeLayer> create(package::Reference ref,
                                                     const RunContext &context);
    ~RuntimeLayer() noexcept;

    struct ExtensionRuntimeLayerInfo
    {
        api::types::v1::ExtensionDefine extensionInfo;
        std::reference_wrapper<RuntimeLayer> extensionLayer;
        std::string forRef;
    };

    utils::error::Result<void>
    resolveLayer(const std::vector<std::string> &modules = {},
                 const std::optional<std::string> &subRef = std::nullopt);

    [[nodiscard]] const api::types::v1::RepositoryCacheLayersItem &getCachedItem() const noexcept
    {
        return cachedItem;
    }

    [[nodiscard]] const package::Reference &getReference() const noexcept { return reference; }

    [[nodiscard]] const std::optional<package::LayerDir> &getLayerDir() const noexcept
    {
        return layerDir;
    }

    void setExtensionInfo(ExtensionRuntimeLayerInfo info) noexcept { extensionOf = info; }

    [[nodiscard]] const std::optional<ExtensionRuntimeLayerInfo> &getExtensionInfo() const noexcept
    {
        return extensionOf;
    }

private:
    RuntimeLayer(package::Reference ref, const RunContext &context);

    package::Reference reference;
    const RunContext *runContext{ nullptr };
    std::optional<package::LayerDir> layerDir;
    api::types::v1::RepositoryCacheLayersItem cachedItem;
    bool temporary{ false };
    std::optional<ExtensionRuntimeLayerInfo> extensionOf;
};

}; // namespace linglong::runtime
