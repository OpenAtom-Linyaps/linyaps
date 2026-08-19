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

TEST_F(RepoCacheTest, QueryUsesEachFilter)
{
    auto cacheFile = tempDir.path() / "states.json";
    RepoCache cache(cacheFile);

    EXPECT_TRUE(cache.addLayerItem(createLayerItem("c1", "app.a", "1.0.0")).has_value());
    EXPECT_TRUE(cache.addLayerItem(createLayerItem("c2", "app.a", "2.0.0")).has_value());
    EXPECT_TRUE(cache.addLayerItem(createLayerItem("c3", "app.b", "1.0.0")).has_value());

    // filter by id
    auto byId = cache.queryLayerItem(repoCacheQuery{ .id = "app.a" });
    ASSERT_EQ(byId.size(), 2);

    // filter by version
    auto byVersion = cache.queryLayerItem(repoCacheQuery{ .id = "app.a", .version = "2.0.0" });
    ASSERT_EQ(byVersion.size(), 1);
    EXPECT_EQ(byVersion.front().commit, "c2");

    // filter by module (none has "runtime" module)
    auto byModule = cache.queryLayerItem(repoCacheQuery{ .id = "app.a", .module = "runtime" });
    EXPECT_TRUE(byModule.empty());

    // filter by deleted flag
    EXPECT_TRUE(cache.addLayerItem(createLayerItem("c4", "app.c", "1.0.0", true)).has_value());
    auto deleted = cache.queryLayerItem(repoCacheQuery{ .deleted = true });
    ASSERT_EQ(deleted.size(), 1);
    EXPECT_EQ(deleted.front().commit, "c4");
    auto notDeleted = cache.queryLayerItem(repoCacheQuery{ .deleted = false });
    EXPECT_EQ(notDeleted.size(), 3);
}

TEST_F(RepoCacheTest, UpdateMergedItemsPersists)
{
    auto cacheFile = tempDir.path() / "states.json";
    RepoCache cache(cacheFile);

    std::vector<api::types::v1::RepositoryCacheMergedItem> items = {
        api::types::v1::RepositoryCacheMergedItem{
          .commits = std::vector<std::string>{ "commit-1" },
          .id = "app.merged",
          .modules = std::vector<std::string>{ "binary" },
        },
    };
    ASSERT_TRUE(cache.updateMergedItems(items).has_value());

    RepoCache reloaded(cacheFile);
    ASSERT_TRUE(reloaded.load().has_value());
    auto merged = reloaded.queryMergedItems();
    ASSERT_TRUE(merged.has_value());
    ASSERT_EQ(merged->size(), 1);
    EXPECT_EQ(merged->at(0).id, "app.merged");
}

TEST_F(RepoCacheTest, AddItemFailsWhenParentMissing)
{
    RepoCache cache(tempDir.path() / "missing" / "states.json");
    auto item = createLayerItem("c1", "app.a", "1.0.0");
    auto result = cache.addLayerItem(item);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RepoCacheTest, AddDuplicateItemFails)
{
    auto cacheFile = tempDir.path() / "states.json";
    RepoCache cache(cacheFile);
    auto item = createLayerItem("c1", "app.a", "1.0.0");
    ASSERT_TRUE(cache.addLayerItem(item).has_value());
    EXPECT_FALSE(cache.addLayerItem(item).has_value());
}

TEST_F(RepoCacheTest, LoadCorruptFileFails)
{
    auto cacheFile = tempDir.path() / "states.json";
    std::ofstream(cacheFile) << "{ not valid json !";
    RepoCache cache(cacheFile);
    EXPECT_FALSE(cache.load().has_value());
}

TEST_F(RepoCacheTest, QueryUsesRemainingFilters)
{
    auto cacheFile = tempDir.path() / "states.json";
    RepoCache cache(cacheFile);

    auto item = createLayerItem("c1", "app.filter", "3.4.5");
    item.info.uuid = "11111111-1111-1111-1111-111111111111";
    item.repo = "alpha";
    item.info.channel = "edge";
    item.info.arch = { "aarch64" };
    ASSERT_TRUE(cache.addLayerItem(item).has_value());

    // filter by repo
    auto byRepo = cache.queryLayerItem(repoCacheQuery{ .repo = "alpha" });
    ASSERT_EQ(byRepo.size(), 1);
    EXPECT_EQ(byRepo.front().commit, "c1");
    EXPECT_TRUE(cache.queryLayerItem(repoCacheQuery{ .repo = "nightly" }).empty());

    // filter by channel
    auto byChannel = cache.queryLayerItem(repoCacheQuery{ .channel = "edge" });
    ASSERT_EQ(byChannel.size(), 1);
    EXPECT_EQ(byChannel.front().commit, "c1");
    EXPECT_TRUE(cache.queryLayerItem(repoCacheQuery{ .channel = "stable" }).empty());

    // filter by architecture
    auto byArch = cache.queryLayerItem(repoCacheQuery{ .architecture = "aarch64" });
    ASSERT_EQ(byArch.size(), 1);
    EXPECT_EQ(byArch.front().commit, "c1");
    EXPECT_TRUE(cache.queryLayerItem(repoCacheQuery{ .architecture = "x86_64" }).empty());

    // filter by uuid
    auto byUuid = cache.queryLayerItem(
      repoCacheQuery{ .uuid = "11111111-1111-1111-1111-111111111111" });
    ASSERT_EQ(byUuid.size(), 1);
    EXPECT_EQ(byUuid.front().commit, "c1");
    // items without a uuid never match a uuid query
    auto second = createLayerItem("c2", "app.plain", "1.0.0");
    ASSERT_TRUE(cache.addLayerItem(second).has_value());
    auto byUuid2 =
      cache.queryLayerItem(repoCacheQuery{ .uuid = "11111111-1111-1111-1111-111111111111" });
    ASSERT_EQ(byUuid2.size(), 1);

    // queryLayerItem returns the highest version first
    auto byId = cache.queryLayerItem(repoCacheQuery{ .id = "app.filter" });
    ASSERT_EQ(byId.size(), 1);
}

} // namespace

} // namespace linglong::repo::test
