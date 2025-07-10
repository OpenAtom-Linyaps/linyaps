/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfigV2.hpp"
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
    std::optional<std::string> architecture;

    auto arch()
    {
        if (!architecture) {
            auto curArch = package::Architecture::currentCPUArchitecture();
            architecture = curArch ? curArch->toString().toStdString() : std::string{"unknown"};
        }

        return *architecture;
    }

    [[nodiscard]] std::string to_string() noexcept
    {
        std::stringstream ss;
        ss << repo.value_or("undefined") << ":" << channel.value_or("undefined") << "/"
           << id.value_or("undefined") << "/" << version.value_or("undefined") << "/" << arch()
           << "/" << module.value_or("undefined");
        return ss.str();
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
           const api::types::v1::RepoConfigV2 &repoConfig,
           OstreeRepo &repo);
    utils::error::Result<void> addLayerItem(const api::types::v1::RepositoryCacheLayersItem &item);
    utils::error::Result<void>
    deleteLayerItem(const api::types::v1::RepositoryCacheLayersItem &item) noexcept;

    [[nodiscard]] std::vector<api::types::v1::RepositoryCacheLayersItem>
    queryLayerItem(const repoCacheQuery &query) const noexcept;

    [[nodiscard]] std::vector<api::types::v1::RepositoryCacheLayersItem>
    queryExistingLayerItem() const noexcept;

    utils::error::Result<void>
    updateMergedItems(const std::vector<api::types::v1::RepositoryCacheMergedItem> &items) noexcept;

    [[nodiscard]] const std::optional<std::vector<api::types::v1::RepositoryCacheMergedItem>> &
    queryMergedItems() const noexcept
    {
        return this->cache.merged;
    }

    utils::error::Result<void> rebuildCache(const api::types::v1::RepoConfigV2 &repoConfig,
                                            OstreeRepo &repo) noexcept;
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
