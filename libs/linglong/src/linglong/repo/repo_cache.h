/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/utils/error/error.h"

#include <ostree.h>

#include <filesystem>
#include <iostream>

namespace linglong::repo {

class RepoCache
{
public:
    static utils::error::Result<RepoCache> create(const std::filesystem::path &repoRoot,
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
    RepoCache(const std::filesystem::path &repoRoot);
    utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>>
    scanAllLayers(const OstreeRepo &repo);
    utils::error::Result<void> rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                            const OstreeRepo &repo);

    api::types::v1::RepositoryCache cache;
    std::string configPath;
};
} // namespace linglong::repo
