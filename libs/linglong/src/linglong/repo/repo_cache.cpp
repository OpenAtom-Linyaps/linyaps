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

utils::error::Result<std::unique_ptr<RepoCache>>
RepoCache::create(const std::filesystem::path &cacheFile,
                  const api::types::v1::RepoConfig &repoConfig,
                  OstreeRepo &repo)
{
    LINGLONG_TRACE("load from RepoCache");

    struct enableMaker : public RepoCache
    {
        using RepoCache::RepoCache;
    };

    // making the constructor of RepoCache be public within this function
    // see also: https://seanmiddleditch.github.io/enabling-make-unique-with-private-constructors
    auto repoCache = std::make_unique<enableMaker>();
    repoCache->cacheFile = cacheFile;
    std::error_code ec;
    if (!std::filesystem::exists(repoCache->cacheFile, ec)) {
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
      QString::fromStdString(repoCache->cacheFile.string()));
    if (!result) {
        std::cout << "invalid cache file, rebuild cache..." << std::endl;
        auto ret = repoCache->rebuildCache(repoConfig, repo);
        if (!ret) {
            return LINGLONG_ERR(ret);
        }
        return repoCache;
    }

    repoCache->cache = std::move(result).value();
    if (repoCache->cache.version != "1" || repoCache->cache.llVersion != LINGLONG_VERSION) {
        std::cout << "The existing cache is outdated, rebuild cache..." << std::endl;
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
                                                   OstreeRepo &repo) noexcept
{
    LINGLONG_TRACE("rebuild repo cache");

    this->cache.llVersion = LINGLONG_VERSION;
    this->cache.config = repoConfig;
    this->cache.version = "1";
    this->cache.migratingStage = std::nullopt;
    this->cache.layers.clear();

    g_autoptr(GHashTable) refsTable = nullptr;
    g_autoptr(GError) gErr = nullptr;
    std::vector<std::string_view> refs;

    if (ostree_repo_list_refs(&repo, nullptr, &refsTable, nullptr, &gErr) == FALSE) {
        return LINGLONG_ERR("ostree_repo_list_refs", gErr);
    }

    // we couldn't report error within below lambda and for_each wouldn't return early if error
    // occurred, copy refs out
    g_hash_table_foreach(
      refsTable,
      [](gpointer key, gpointer value, gpointer data) {
          // key,value -> ref,checksum
          auto *vec = static_cast<std::vector<std::string_view> *>(data);
          vec->emplace_back(static_cast<const char *>(key));
      },
      &refs);

    bool refsNeedMigrate{ false };
    for (auto ref : refs) {
        auto pos = ref.find(':');
        if (pos == std::string::npos) {
            refsNeedMigrate = true;
            qWarning() << "invalid ref: " << ref.data();
            continue;
        }

        api::types::v1::RepositoryCacheLayersItem item;
        item.repo = ref.substr(0, pos);

        g_autofree char *commit{ nullptr };
        g_autoptr(GError) gErr{ nullptr };
        g_autoptr(GFile) root{ nullptr };
        if (ostree_repo_read_commit(&repo, ref.data(), &root, &commit, nullptr, &gErr) == FALSE) {
            return LINGLONG_ERR("ostree_repo_read_commit", gErr);
        }
        item.commit = commit;

        // ostree ls --repo repo ref, the file path of info.json is /info.json.
        g_autoptr(GFile) infoFile = g_file_resolve_relative_path(root, "info.json");
        auto info = utils::parsePackageInfo(infoFile);
        if (!info) {
            return LINGLONG_ERR(info);
        }
        item.info = *info;

        this->cache.layers.emplace_back(std::move(item));
    }

    if (refsNeedMigrate) {
        if (!this->cache.migratingStage) {
            this->cache.migratingStage = std::vector<int64_t>{};
        }

        this->cache.migratingStage->emplace_back(
          static_cast<int64_t>(MigrationStage::RefsWithoutRepo));
    }

    auto ret = writeToDisk();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return LINGLONG_OK;
}

std::optional<std::vector<MigrationStage>> RepoCache::migrations() const noexcept
{
    if (!cache.migratingStage) {
        return std::nullopt;
    }

    std::vector<MigrationStage> stages;
    for (auto stage : cache.migratingStage.value()) {
        stages.emplace_back(static_cast<MigrationStage>(stage));
    }

    return stages;
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
                   || item.info.version != val.info.version
                   || item.info.arch.front() != val.info.arch.front()
                   || item.info.packageInfoV2Module != val.info.packageInfoV2Module);
      });

    if (it != cache.layers.end()) {
        assert(false);
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
RepoCache::deleteLayerItem(const api::types::v1::RepositoryCacheLayersItem &item) noexcept
{
    LINGLONG_TRACE("delete layer item");

    auto it = std::find_if(
      cache.layers.begin(),
      cache.layers.end(),
      [&item](const api::types::v1::RepositoryCacheLayersItem &val) {
          return !(item.commit != val.commit || item.repo != val.repo
                   || item.info.channel != val.info.channel || item.info.id != val.info.id
                   || item.info.version != val.info.version
                   || item.info.arch.front() != val.info.arch.front()
                   || item.info.packageInfoV2Module != val.info.packageInfoV2Module);
      });

    if (it == cache.layers.end()) {
        assert(false);
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
RepoCache::queryLayerItem(const repoCacheQuery &query) const noexcept
{
    using itemRef = std::reference_wrapper<const api::types::v1::RepositoryCacheLayersItem>;
    std::vector<itemRef> layers_view;
    for (const auto &layer : cache.layers) {
        if (query.id && query.id.value() != layer.info.id) {
            continue;
        }

        if (query.repo && query.repo.value() != layer.repo) {
            continue;
        }

        if (query.channel && query.channel.value() != layer.info.channel) {
            continue;
        }

        if (query.version && query.version.value() != layer.info.version) {
            continue;
        }

        if (query.module && query.module.value() != layer.info.packageInfoV2Module) {
            continue;
        }

        if (query.uuid) {
            if (!layer.info.uuid) {
                continue;
            }

            if (query.uuid.value() != layer.info.uuid.value()) {
                continue;
            }
        }

        layers_view.emplace_back(layer);
    }

    std::sort(layers_view.begin(), layers_view.end(), [](itemRef lhs, itemRef rhs) {
        return lhs.get().info.version > rhs.get().info.version;
    });

    return { layers_view.cbegin(), layers_view.cend() };
}

utils::error::Result<void> RepoCache::writeToDisk()
{
    LINGLONG_TRACE("save repo cache");

    std::error_code ec;
    if (!std::filesystem::exists(this->cacheFile.parent_path(), ec)) {
        return LINGLONG_ERR("The parent directory of state.json doesn't exist:"
                            + QString::fromStdString(ec.message()));
    }

    auto ofs = std::ofstream(this->cacheFile, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) { // dump all info
        auto dumpStatus = [](const std::filesystem::path &p, std::error_code &ec) {
            LINGLONG_TRACE("dump status")
            auto status = std::filesystem::status(p, ec);
            if (ec) {
                return;
            }

            auto targetPerm = status.permissions();

            using std::filesystem::perms;
            auto out = qInfo().nospace();
            out << QString::fromStdString(p.string()) << ":";
            auto show = [&out, targetPerm](char op, perms perm) {
                out << (perms::none == (perm & targetPerm) ? '-' : op);
            };
            show('r', perms::owner_read);
            show('w', perms::owner_write);
            show('x', perms::owner_exec);
            show('r', perms::group_read);
            show('w', perms::group_write);
            show('x', perms::group_exec);
            show('r', perms::others_read);
            show('w', perms::others_write);
            show('x', perms::others_exec);
        };

        qInfo() << "process uid:" << ::getuid() << "process gid:" << ::getgid();

        dumpStatus(this->cacheFile.parent_path(), ec);
        if (ec) {
            QString msg = "get status of directory"
              + QString::fromStdString(this->cacheFile.parent_path())
              + "error:" + QString::fromStdString(ec.message());
            return LINGLONG_ERR(msg);
        }

        if (std::filesystem::exists(this->cacheFile, ec)) {
            dumpStatus(this->cacheFile, ec);
            if (ec) {
                QString msg = "get status of file"
                  + QString::fromStdString(this->cacheFile.string())
                  + "error:" + QString::fromStdString(ec.message());
                return LINGLONG_ERR(msg);
            }
        }

        if (ec) {
            QString msg = "check file" + QString::fromStdString(this->cacheFile.string())
              + "exist error:" + QString::fromStdString(ec.message());
            return LINGLONG_ERR(msg);
        }
    }

    auto data = nlohmann::json(this->cache).dump();
    ofs << data;

    return LINGLONG_OK;
}

} // namespace linglong::repo
