/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_cache.h"

#include "linglong/utils/configure.h"
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
                  const OstreeRepo &repo)
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

    // update repo config
    repoCache->cache.config = repoConfig;
    return repoCache;
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

utils::error::Result<void>
RepoCache::addLayerItem(const cacheRef &ref, const api::types::v1::RepositoryCacheLayersItem &item)
{
    LINGLONG_TRACE("add layer item");

    if (!ref.version || !ref.repo || !ref.channel || !ref.module || ref.id.empty()) {
        return LINGLONG_ERR("unqualified cache ref");
    }

    std::string cacheRef = ref.repo.value() + ":" + ref.channel.value() + "/" + ref.id + "/"
      + ref.version.value() + linglong::repo::cacheRef::arch + ref.module.value();

    if (ref.uuid) {
        cacheRef += "_" + ref.uuid.value();
    }

    cache.layers.emplace(std::move(cacheRef), item);
    return LINGLONG_OK;
}

utils::error::Result<void> RepoCache::deleteLayerItem(const cacheRef &ref)
{
    LINGLONG_TRACE("delete layer item");

    if (!ref.version || !ref.repo || !ref.channel || !ref.module || ref.id.empty()) {
        return LINGLONG_ERR("unqualified cache ref");
    }

    std::string cacheRef = ref.repo.value() + ":" + ref.channel.value() + "/" + ref.id + "/"
      + ref.version.value() + linglong::repo::cacheRef::arch + ref.module.value();

    if (ref.uuid) {
        cacheRef += "_" + ref.uuid.value();
    }

    cache.layers.erase(cacheRef);
    return LINGLONG_OK;
}

utils::error::Result<std::vector<package::Reference>>
RepoCache::searchLayerItem(const cacheRef &ref)
{
    LINGLONG_TRACE("search layer item");

    std::vector<package::Reference> refs;
    for (const auto &[ref, item] : cache.layers) {
        // format: repo:channel/id/version/arch/module[_uuid]
        auto location = ref.find(':');
        if (location == std::string::npos) {
            qCritical() << "invalid content of cache ref:" << QString::fromStdString(ref);
            continue;
        }

        std::string channel;
        std::string id;
        std::string version;

        auto newRef = ref.substr(location + 1); // no need to match 'repo' for now
        std::istringstream stream{ newRef };
        std::string component;
        while (std::getline(stream, component, '/')) {
            if (channel.empty()) {
                channel = std::move(component);
                continue;
            }

            if (id.empty()) {
                id = std::move(component);
                continue;
            }

            if (version.empty()) {
                version = std::move(component);
                continue;
            }

            break;
        }

        auto packageVer = package::Version::parse(QString::fromStdString(version));
        if (!packageVer) {
            qCritical() << packageVer.error() << ":" << QString::fromStdString(ref);
            continue;
        }

        auto arch = package::Architecture::currentCPUArchitecture();
        if (!arch) {
            qCritical() << arch.error() << ":" << QString::fromStdString(ref);
            continue;
        }

        auto packageRef = package::Reference::create(QString::fromStdString(channel),
                                                     QString::fromStdString(id),
                                                     *packageVer,
                                                     *arch);
        if (!packageRef) {
            qCritical() << packageRef.error() << QString::fromStdString(ref);
            continue;
        }

        refs.emplace_back(std::move(packageRef).value());
    }

    auto excludeFilter = [&refs](auto pred) {
        static_assert(std::is_invocable_r_v<bool, decltype(pred), const package::Reference &>);
        refs.erase(std::remove_if(refs.begin(), refs.end(), pred), std::end(refs));
    };

    excludeFilter([&ref](const package::Reference &pkgRef) {
        return ref.id != pkgRef.id.toStdString();
    });

    if (ref.channel) {
        excludeFilter([&ref](const package::Reference &pkgRef) {
            return ref.channel != pkgRef.channel.toStdString();
        });
    }

    if (ref.version) {
        excludeFilter([&ref](const package::Reference &pkgRef) {
            return ref.channel != pkgRef.channel.toStdString();
        });
    }

    std::sort(refs.begin(),
              refs.end(),
              [](const package::Reference &lhs, const package::Reference &rhs) {
                  return lhs.version > rhs.version;
              });

    return refs;
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
