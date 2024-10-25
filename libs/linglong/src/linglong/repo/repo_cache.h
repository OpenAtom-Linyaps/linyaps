/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/api/types/v1/RepositoryCacheMergedItem.hpp"
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
    std::optional<bool> deleted;

    static auto arch()
    {
        auto ret = package::Architecture::currentCPUArchitecture();
        if (ret) {
            return ret->toString().toStdString();
        }

        return std::string{ "unknown" };
    }
};

enum class MigrationStage : int64_t { RefsWithoutRepo };

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

    [[nodiscard]] std::vector<api::types::v1::RepositoryCacheLayersItem>
    queryLayerItem(const repoCacheQuery &query) const noexcept;

    [[nodiscard]] const std::vector<api::types::v1::RepositoryCacheLayersItem> &
    queryLayerItem() const noexcept
    {
        return this->cache.layers;
    }

    utils::error::Result<void>
    updateMergedItems(const std::vector<api::types::v1::RepositoryCacheMergedItem> &items) noexcept;

    [[nodiscard]] const std::optional<std::vector<api::types::v1::RepositoryCacheMergedItem>> &
    queryMergedItems() const noexcept
    {
        return this->cache.merged;
    }

    utils::error::Result<void> rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                            OstreeRepo &repo) noexcept;
    [[nodiscard]] std::optional<std::vector<MigrationStage>> migrations() const noexcept;
    utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>::iterator>
    findMatchingItem(const api::types::v1::RepositoryCacheLayersItem &item) noexcept;
    utils::error::Result<void> writeToDisk();

private:
    RepoCache() = default;
    static constexpr auto cacheFileVersion = "2";
    api::types::v1::RepositoryCache cache;
    std::filesystem::path cacheFile;
};
} // namespace linglong::repo
