/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/RepositoryCache.hpp"
#include "linglong/repo/repo_cache.h"

#include <filesystem>
#include <fstream>

namespace linglong::repo::test {

namespace fs = std::filesystem;

namespace {

api::types::v1::RepoConfigV2 createRepoConfig()
{
    return api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .name = "stable",
                                         .priority = 0,
                                         .url = "https://example.com/repo" } },
        .version = 2,
    };
}

api::types::v1::PackageInfoV2 createPackageInfo(std::string id, std::string version)
{
    return api::types::v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = std::move(id),
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = std::move(version),
    };
}

api::types::v1::RepositoryCacheLayersItem
createLayerItem(std::string commit,
                std::string id,
                std::string version,
                std::optional<bool> deleted = std::nullopt)
{
    return api::types::v1::RepositoryCacheLayersItem{
        .commit = std::move(commit),
        .deleted = deleted,
        .info = createPackageInfo(std::move(id), std::move(version)),
        .repo = "stable",
    };
}

void writeCacheFile(const fs::path &path, const api::types::v1::RepositoryCache &cache)
{
    nlohmann::json data = cache;
    std::ofstream(path) << data.dump();
}

class RepoCacheTest : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(RepoCacheTest, loadFailsWhenCacheFileIsMissing)
{
    ASSERT_TRUE(tempDir.isValid());

    RepoCache cache(tempDir.path() / "states.json");
    auto result = cache.load();

    EXPECT_FALSE(result.has_value());
}

TEST_F(RepoCacheTest, loadRejectsVersionMismatch)
{
    ASSERT_TRUE(tempDir.isValid());

    auto cacheFile = tempDir.path() / "states.json";
    writeCacheFile(cacheFile,
                   api::types::v1::RepositoryCache{
                     .config = createRepoConfig(),
                     .layers = {},
                     .llVersion = "test",
                     .merged = std::nullopt,
                     .version = "1",
                   });

    RepoCache cache(cacheFile);
    auto result = cache.load();

    EXPECT_FALSE(result.has_value());
}

TEST_F(RepoCacheTest, queryExistingLayerItemSkipsDeletedEntriesAfterLoad)
{
    ASSERT_TRUE(tempDir.isValid());

    auto cacheFile = tempDir.path() / "states.json";
    writeCacheFile(cacheFile,
                   api::types::v1::RepositoryCache{
                     .config = createRepoConfig(),
                     .layers = { createLayerItem("commit-live", "app.live", "1.0.0"),
                                 createLayerItem("commit-deleted", "app.deleted", "1.0.0", true) },
                     .llVersion = "test",
                     .merged = std::nullopt,
                     .version = "2",
                   });

    RepoCache cache(cacheFile);
    ASSERT_TRUE(cache.load().has_value());

    auto items = cache.queryExistingLayerItem();
    ASSERT_EQ(items.size(), 1);
    EXPECT_EQ(items.front().commit, "commit-live");
    EXPECT_EQ(items.front().info.id, "app.live");
}

TEST_F(RepoCacheTest, addAndDeleteLayerItemPersistAcrossReload)
{
    ASSERT_TRUE(tempDir.isValid());

    auto cacheFile = tempDir.path() / "states.json";
    RepoCache cache(cacheFile);
    auto item = createLayerItem("commit-1", "app.test", "1.0.0");

    ASSERT_TRUE(cache.addLayerItem(item).has_value());

    RepoCache reloaded(cacheFile);
    ASSERT_TRUE(reloaded.load().has_value());
    auto loadedItems = reloaded.queryLayerItem(repoCacheQuery{ .id = "app.test" });
    ASSERT_EQ(loadedItems.size(), 1);
    EXPECT_EQ(loadedItems.front().commit, "commit-1");

    ASSERT_TRUE(reloaded.deleteLayerItem(item).has_value());

    RepoCache afterDelete(cacheFile);
    ASSERT_TRUE(afterDelete.load().has_value());
    EXPECT_TRUE(afterDelete.queryLayerItem(repoCacheQuery{ .id = "app.test" }).empty());
}

} // namespace

} // namespace linglong::repo::test
