/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_cache.h"

#include "linglong/utils/configure.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/serialize/json.h"

#include <fstream>
#include <iostream>

namespace linglong::repo {

RepoCache::RepoCache(RepoCache &&other) noexcept
    : cache(std::move(other).cache)
    , configPath(std::move(other).configPath)
{
}

RepoCache &RepoCache::operator=(RepoCache &&other) noexcept
{
    if (&other == this) {
        return *this;
    }

    this->cache = std::move(other).cache;
    this->configPath = std::move(other).configPath;
    return *this;
}

utils::error::Result<std::unique_ptr<RepoCache>>
RepoCache::create(const std::filesystem::path &repoRoot,
                  const api::types::v1::RepoConfig &repoConfig,
                  OstreeRepo &repo)
{
    LINGLONG_TRACE("create RepoCache");

    struct enableMaker : public RepoCache
    {
        using RepoCache::RepoCache;
    };

    auto repoCache = std::make_unique<enableMaker>();
    repoCache->configPath = repoRoot / "cache.json";
    std::error_code ec;
    if (!std::filesystem::exists(repoCache->configPath, ec)) {
        if (ec) {
            std::string error = "checking file existence failed: " + ec.message();
            return LINGLONG_ERR(error.c_str());
        }
        auto ret = repoCache->rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    auto result = utils::serialize::LoadJSONFile<api::types::v1::RepositoryCache>(
      QString::fromStdString(repoCache->configPath));
    if (!result) {
        std::cout << "invalid cache file, rebuild cache..." << std::endl;
        auto ret = repoCache->rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    repoCache->cache = std::move(result).value();
    std::string cliVersion = LINGLONG_VERSION;
    if (result->llVersion > cliVersion) {
        std::cout << "linglong cli version is older than cache file, rebuild cache..." << std::endl;
        auto ret = repoCache->rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    if (repoCache->isLayerEmpty()) {
        std::cout << "layer in cache is empty, try to rebuild cache..." << std::endl;
        auto ret = repoCache->rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    // update repo config
    repoCache->cache.config = repoConfig;
    return repoCache;
}

bool RepoCache::isLayerEmpty() const noexcept
{
    return this->cache.layers.empty();
}

utils::error::Result<void> RepoCache::rebuildCache(const api::types::v1::RepoConfig &repoConfig,
                                                   OstreeRepo &repo)
{
    LINGLONG_TRACE("rebuild repo cache");

    this->cache.llVersion = LINGLONG_VERSION;
    this->cache.config = repoConfig;
    this->cache.version = "0.0.1";

    g_autoptr(GHashTable) refsTable = nullptr;
    g_autoptr(GError) gErr = nullptr;
    std::vector<std::string> refs;

    if (!ostree_repo_list_refs(&repo, nullptr, &refsTable, nullptr, &gErr)) {
        return LINGLONG_ERR("ostree_repo_list_refs", gErr);
    }

    g_hash_table_foreach(
      refsTable,
      [](gpointer key, gpointer value, gpointer data) {
          // key,value -> ref,checksum
          std::vector<std::string> *vec = static_cast<std::vector<std::string> *>(data);
          vec->emplace_back(static_cast<char *>(key));
      },
      &refs);

    for (const auto ref : refs) {
        g_autofree char *commit = nullptr;
        g_autoptr(GError) gErr = nullptr;
        g_autoptr(GFile) root = nullptr;
        api::types::v1::RepositoryCacheLayersItem item;

        g_clear_error(&gErr);
        if (!ostree_repo_read_commit(&repo, ref.c_str(), &root, &commit, nullptr, &gErr)) {
            return LINGLONG_ERR("ostree_repo_read_commit", gErr);
        }

        // ostree ls --repo repo ref, the file path of info.json is /info.json.
        g_autoptr(GFile) infoFile = g_file_resolve_relative_path(root, "info.json");

        auto info = utils::parsePackageInfo(infoFile);
        if (!info) {
            return LINGLONG_ERR(info);
        }

        item.commit = commit;
        item.info = *info;
        size_t pos = ref.find(":");
        if (pos != std::string::npos) {
            item.repo = ref.substr(0, pos);
        } else {
            std::cout << "invalid ref: " << ref;
            continue;
        }
        this->cache.layers.emplace_back(item);
    }

    auto ret = writeToDisk();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
RepoCache::addLayerItem(const api::types::v1::RepositoryCacheLayersItem &item)
{
    LINGLONG_TRACE("add layer item");

    auto it = std::find_if(
      cache.layers.begin(),
      cache.layers.end(),
      [&item](const api::types::v1::RepositoryCacheLayersItem &val) {
          return !(item.commit != val.commit || item.repo != val.repo
                   || item.info.channel != val.info.channel || item.info.id != val.info.id
                   || item.info.version != val.info.version || item.info.arch != val.info.arch
                   || item.info.packageInfoV2Module != val.info.packageInfoV2Module);
      });

    if (it != cache.layers.end()) {
        return LINGLONG_ERR("item already exist");
    }

    cache.layers.emplace_back(item);
    auto ret = writeToDisk();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
RepoCache::deleteLayerItem(const api::types::v1::RepositoryCacheLayersItem &item)
{
    LINGLONG_TRACE("delete layer item");

    auto it = std::find_if(
      cache.layers.begin(),
      cache.layers.end(),
      [&item](const api::types::v1::RepositoryCacheLayersItem &val) {
          return !(item.commit != val.commit || item.repo != val.repo
                   || item.info.channel != val.info.channel || item.info.id != val.info.id
                   || item.info.version != val.info.version || item.info.arch != val.info.arch
                   || item.info.packageInfoV2Module != val.info.packageInfoV2Module);
      });

    if (it == cache.layers.end()) {
        return LINGLONG_ERR("item doesn't exist");
    }

    cache.layers.erase(it);
    auto ret = writeToDisk();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

std::vector<api::types::v1::RepositoryCacheLayersItem>
RepoCache::searchLayerItem(const CacheRef &ref) const
{
    using itemRef = std::reference_wrapper<const api::types::v1::RepositoryCacheLayersItem>;
    std::vector<itemRef> layers;
    for (const auto &layer : cache.layers) {
        // if id is empty, return all items.
        if (ref.id.empty()) {
            layers.emplace_back(layer);
            continue;
        }

        if (ref.id != layer.info.id) {
            continue;
        }

        if (ref.repo && ref.repo.value() != layer.repo) {
            continue;
        }

        if (ref.channel && ref.channel.value() != layer.info.channel) {
            continue;
        }

        if (ref.version && ref.version.value() != layer.info.version) {
            continue;
        }

        if (ref.module && ref.module.value() != layer.info.packageInfoV2Module) {
            continue;
        }

        if (ref.uuid) {
            if (!layer.info.uuid) {
                continue;
            }

            if (ref.uuid.value() != layer.info.uuid.value()) {
                continue;
            }
        }

        layers.emplace_back(layer);
    }

    std::sort(layers.begin(), layers.end(), [](itemRef lhs, itemRef rhs) {
        return lhs.get().info.version > rhs.get().info.version;
    });

    return std::vector<api::types::v1::RepositoryCacheLayersItem>(layers.cbegin(), layers.cend());
}

utils::error::Result<void> RepoCache::writeToDisk()
{
    LINGLONG_TRACE("save repo cache");

    auto ofs = std::ofstream(this->configPath, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) {
        return LINGLONG_ERR("open failed");
    }

    auto data = nlohmann::json(this->cache).dump();
    ofs << data;

    return LINGLONG_OK;
}

} // namespace linglong::repo
