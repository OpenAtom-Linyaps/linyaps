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
    , temporary(false)
{
    const auto &repo = context.getRepo();
    auto item = repo.getLayerItem(reference);
    if (!item) {
        throw std::runtime_error("no cached item found");
    }
    cachedItem = std::move(item).value();
}

RuntimeLayer::~RuntimeLayer() noexcept
{
    if (temporary && layerDir) {
        std::error_code ec;
        const auto &path = layerDir->path();
        std::filesystem::remove_all(path, ec);
        if (ec) {
            LogI("failed to remove all files under {}: {}", path, ec.message());
        }
    }
}

utils::error::Result<void>
RuntimeLayer::resolveLayer(const std::optional<std::vector<std::string>> &includeModules,
                           const std::optional<std::vector<std::string>> &excludeModules,
                           const std::optional<std::string> &subRef)
{
    LINGLONG_TRACE("resolve layer");

    if (this->runContext == nullptr) {
        return LINGLONG_ERR("runContext is nullptr");
    }

    auto &repo = runContext->getRepo();
    utils::error::Result<package::LayerDir> layer(LINGLONG_ERR("null"));
    if (!includeModules && !excludeModules) {
        layer = repo.getMergedModuleDir(reference, true, subRef);
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
            layer = repo.getMergedModuleDir(reference, true, subRef);
        } else if (modules.size() == 1) {
            layer = repo.getLayerDir(reference, modules.front(), subRef);
        } else {
            layer = repo.createTempMergedModuleDir(reference, modules);
            if (!layer) {
                return LINGLONG_ERR(layer);
            }
            temporary = true;
            LogD("create temp merged module dir: {}", layer->path());
        }
    }

    if (!layer) {
        return LINGLONG_ERR("layer doesn't exist: " + reference.toString(), layer);
    }

    layerDir = *layer;
    return LINGLONG_OK;
}
} // namespace linglong::runtime
