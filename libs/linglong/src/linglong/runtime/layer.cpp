// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/runtime/layer.h"

#include "linglong/runtime/run_context.h"
#include "linglong/utils/log/log.h"

#include <fmt/ranges.h>

#include <algorithm>

namespace linglong::runtime {
utils::error::Result<RuntimeLayer> RuntimeLayer::create(package::Reference ref,
                                                        const RunContext &context)
{
    LINGLONG_TRACE(fmt::format("create runtime layer from ref {}", ref.toString()));

    try {
        return RuntimeLayer(std::move(ref), context);
    } catch (const std::exception &e) {
        return LINGLONG_ERR("failed to create runtime layer", e);
    }
}

RuntimeLayer::RuntimeLayer(package::Reference ref, const RunContext &context)
    : reference(std::move(ref))
    , runContext(&context)
{
    const auto &repo = context.getRepo();
    auto item = repo.getLayerItem(reference);
    if (!item) {
        throw std::runtime_error("no cached item found");
    }
    cachedItem = std::move(item).value();
}

utils::error::Result<void>
RuntimeLayer::resolveLayer(const std::optional<std::vector<std::string>> &includeModules,
                           const std::optional<std::vector<std::string>> &excludeModules)
{
    LINGLONG_TRACE("resolve layer");

    if (this->runContext == nullptr) {
        return LINGLONG_ERR("runContext is nullptr");
    }

    auto &repo = runContext->getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    std::optional<package::TempLayerDir> resolvedTempLayer;
    if (!includeModules && !excludeModules) {
        layer = repo.getMergedModuleDir(reference, true);
    } else {
        auto normalizeModules = [](std::vector<std::string> modules) {
            std::sort(modules.begin(), modules.end());
            modules.erase(std::unique(modules.begin(), modules.end()), modules.end());
            return modules;
        };

        const auto installedModules = normalizeModules(repo.getModuleList(reference));
        auto modules = includeModules ? normalizeModules(*includeModules) : installedModules;
        if (excludeModules) {
            const auto &excluded = *excludeModules;
            auto it = std::remove_if(modules.begin(), modules.end(), [&excluded](const auto &m) {
                return std::find(excluded.begin(), excluded.end(), m) != excluded.end();
            });
            modules.erase(it, modules.end());
        }
        if (modules.empty()) {
            return LINGLONG_ERR("no modules selected to resolve");
        }

        if (modules == installedModules) {
            layer = repo.getMergedModuleDir(reference, true);
        } else if (modules.size() == 1) {
            layer = repo.getLayerDir(reference, modules.front());
        } else {
            auto mergedLayer = repo.createTempMergedModuleDir(reference, modules);
            if (!mergedLayer) {
                return LINGLONG_ERR(mergedLayer);
            }
            resolvedTempLayer = std::move(*mergedLayer);
            layer = resolvedTempLayer->layerDir();
            LogD("create temp merged module dir: {}", resolvedTempLayer->path());
        }
    }

    if (!layer) {
        return LINGLONG_ERR("layer doesn't exist: " + reference.toString(), layer);
    }

    layerDir = *layer;
    tempLayerDir = std::move(resolvedTempLayer);
    return LINGLONG_OK;
}
} // namespace linglong::runtime
