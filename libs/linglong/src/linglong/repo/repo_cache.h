/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <filesystem>

namespace linglong::repo {

class RepoCache
{
public:
    RepoCache(const RepoCache &) = delete;
    RepoCache &operator=(const RepoCache &) = delete;
    RepoCache(RepoCache &&other) noexcept;
    RepoCache &operator=(RepoCache &&other) noexcept;
    ~RepoCache() = default;

    static utils::error::Result<std::unique_ptr<RepoCache>>
    create(const std::filesystem::path &repoRoot,
           const api::types::v1::RepoConfig &repoConfig,
           const OstreeRepo &repo);
    utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>::iterator>
    addLayerItem(const std::string &ref, const api::types::v1::RepositoryCacheLayersItem &item);
    utils::error::Result<void> deleteLayerItem(const std::string &ref);
    utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>::iterator>
    updateLayerItem(const std::string &ref, const api::types::v1::RepositoryCacheLayersItem &item);
    utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
    searchLayerItem(const std::string &ref);

    utils::error::Result<void> writeToDisk();

private:
    RepoCache() = default;
    utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>>
    scanAllLayers(const OstreeRepo &repo);
    utils::error::Result<void> rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                            const OstreeRepo &repo);

    api::types::v1::RepositoryCache cache;
    std::string configPath;
};
} // namespace linglong::repo
