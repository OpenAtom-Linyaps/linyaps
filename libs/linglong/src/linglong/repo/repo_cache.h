/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/package/architecture.h"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <filesystem>

namespace linglong::repo {

struct repoCacheQuery
{
    std::optional<std::string> id;
    std::optional<std::string> repo; // not used currently
    std::optional<std::string> channel;
    std::optional<std::string> version; // could be fuzzy version
    std::optional<std::string> module;
    std::optional<std::string> uuid;

    static auto arch()
    {
        auto ret = package::Architecture::currentCPUArchitecture();
        if (ret) {
            return ret->toString().toStdString();
        }

        return std::string{ "unknown" };
    }
};

class RepoCache
{
public:
    RepoCache(const RepoCache &) = delete;
    RepoCache &operator=(const RepoCache &) = delete;
    RepoCache(RepoCache &&other) = delete;
    RepoCache &operator=(RepoCache &&other) = delete;
    ~RepoCache() = default;

    static utils::error::Result<std::unique_ptr<RepoCache>>
    create(const std::filesystem::path &cacheFile,
           const api::types::v1::RepoConfig &repoConfig,
           OstreeRepo &repo);
    utils::error::Result<void> addLayerItem(const api::types::v1::RepositoryCacheLayersItem &item);
    utils::error::Result<void>
    deleteLayerItem(const api::types::v1::RepositoryCacheLayersItem &item) noexcept;
    utils::error::Result<void>
    updateLayerItem(const api::types::v1::RepositoryCacheLayersItem &item) noexcept;

    [[nodiscard]] std::vector<api::types::v1::RepositoryCacheLayersItem>
    queryLayerItem(const repoCacheQuery &query) const noexcept;

    [[nodiscard]] const std::vector<api::types::v1::RepositoryCacheLayersItem> &
    queryLayerItem() const noexcept
    {
        return this->cache.layers;
    }

    utils::error::Result<void> rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                            OstreeRepo &repo) noexcept;

private:
    RepoCache() = default;
    utils::error::Result<void> writeToDisk();

    api::types::v1::RepositoryCache cache;
    std::filesystem::path cacheFile;
};
} // namespace linglong::repo
