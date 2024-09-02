/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_cache.h"

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/utils/configure.h"
#include "linglong/utils/serialize/json.h"

#include <fstream>

namespace linglong::repo {

utils::error::Result<RepoCache> RepoCache::create(const std::filesystem::path &repoRoot,
                                                  const api::types::v1::RepoConfig &repoConfig,
                                                  const OstreeRepo &repo)
{
    LINGLONG_TRACE("create RepoCache");

    RepoCache repoCache(repoRoot);
    std::error_code ec;
    if (!std::filesystem::exists(repoCache.configPath, ec)) {
        if (ec) {
            std::string error = "checking file existence failed: " + ec.message();
            return LINGLONG_ERR(error.c_str());
        }
        auto ret = repoCache.rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    auto result = utils::serialize::LoadJSONFile<api::types::v1::RepositoryCache>(
      QString::fromStdString(repoCache.configPath));
    if (!result) {
        std::cout << "invalid cache file, rebuild cache..." << std::endl;
        auto ret = repoCache.rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    repoCache.cache = *result;
    std::string cliVersion = LINGLONG_VERSION;
    if (result->llVersion > cliVersion) {
        std::cout << "linglong cli version is older than cache file, rebuild cache..." << std::endl;
        auto ret = repoCache.rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    // update repo config
    repoCache.cache.config = repoConfig;

    return repoCache;
}

RepoCache::RepoCache(const std::filesystem::path &repoRoot)
    : configPath(repoRoot / "cache.json")
    , cache({})
{
}

utils::error::Result<void> RepoCache::rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                                   const OstreeRepo &repo)
{
    LINGLONG_TRACE("rebuild repo cache");
    this->cache.llVersion = LINGLONG_VERSION;
    this->cache.config = repoConfig;
    this->cache.version = "0.0.1";
    this->cache.layers = {}; // scanf all layer
}

utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>>
scanAllLayers(const OstreeRepo &repo)
{
}

utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>::iterator>
RepoCache::addLayerItem(const std::string &ref,
                        const api::types::v1::RepositoryCacheLayersItem &item)
{
    LINGLONG_TRACE("add layer item");
    auto it = this->cache.layers.insert(std::make_pair(ref, item));
    if (!it.second) {
        return LINGLONG_ERR("already exists");
    }

    return it.first;
}

utils::error::Result<void> RepoCache::deleteLayerItem(const std::string &ref)
{
    LINGLONG_TRACE("delete layer item");

    auto it = this->cache.layers.erase(ref);
    if (it <= 0) {
        return LINGLONG_ERR("not found");
    }

    return LINGLONG_OK;
}

utils::error::Result<std::map<std::string, api::types::v1::RepositoryCacheLayersItem>::iterator>
RepoCache::updateLayerItem(const std::string &ref,
                           const api::types::v1::RepositoryCacheLayersItem &item)
{
    LINGLONG_TRACE("update layer item");
    auto it = this->cache.layers.find(ref);
    if (it != this->cache.layers.end()) {
        it->second = item;
        return it;
    }

    return LINGLONG_ERR("not found");
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
RepoCache::searchLayerItem(const std::string &ref)
{
    LINGLONG_TRACE("search layer item");

    auto it = this->cache.layers.find(ref);
    if (it != this->cache.layers.end()) {
        return it->second;
    }

    return LINGLONG_ERR("not found");
}

utils::error::Result<void> RepoCache::writeToDisk()
{
    LINGLONG_TRACE("save repo cache to");

    auto ofs = std::ofstream(this->configPath, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) {
        return LINGLONG_ERR("open failed");
    }

    auto data = nlohmann::json(this->cache).dump();
    ofs << data;

    return LINGLONG_OK;
}

} // namespace linglong::repo
