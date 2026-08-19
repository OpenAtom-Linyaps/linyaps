// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>

namespace linglong::repo::test {

namespace fs = std::filesystem;

namespace {

using linglong::api::types::v1::PackageInfoV2;
using linglong::api::types::v1::Repo;
using linglong::api::types::v1::RepoConfigV2;
using linglong::package::FuzzyReference;
using linglong::package::LayerDir;
using linglong::package::Reference;
using linglong::repo::OSTreeRepo;

RepoConfigV2 makeRepoConfig()
{
    return RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };
}

class RealRepoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        ASSERT_TRUE(tempDir->isValid());
        repoRoot = tempDir->path() / "repo-root";
        ASSERT_TRUE(fs::create_directories(repoRoot));

        auto created = OSTreeRepo::create(repoRoot, makeRepoConfig());
        ASSERT_TRUE(created.has_value()) << created.error().message();
        repo = std::move(*created);

        appRef = importLayer("com.example.app",
                             "1.0.0",
                             "app",
                             "binary",
                             { "files/usr/bin/hello",
                               "files/share/applications/com.example.app.desktop" });
        ASSERT_TRUE(appRef.has_value()) << "failed to import app layer";
        runtimeRef = importLayer("org.deepin.runtime",
                                 "23",
                                 "runtime",
                                 "binary",
                                 { "files/usr/lib/libruntime.so",
                                   "files/lib/systemd/user/org.deepin.runtime.service" });
        ASSERT_TRUE(runtimeRef.has_value()) << "failed to import runtime layer";
    }

    // Creates a layer directory on disk (info.json + files), commits it into
    // the real ostree repo, and returns the resulting local reference.
    std::optional<Reference> importLayer(const std::string &id,
                                         const std::string &version,
                                         const std::string &kind,
                                         const std::string &module,
                                         const std::vector<std::string> &relFiles)
    {
        std::error_code ec;
        auto layerDir = repoRoot / ("layersrc-" + id);
        fs::remove_all(layerDir, ec);
        ec.clear();
        for (const auto &rel : relFiles) {
            auto p = layerDir / rel;
            fs::create_directories(p.parent_path(), ec);
            if (ec) {
                ADD_FAILURE() << "create dirs failed: " << ec.message();
                return std::nullopt;
            }
            std::ofstream{ p } << "test content";
        }

        PackageInfoV2 info;
        info.id = id;
        info.version = version;
        info.kind = kind;
        info.channel = "stable";
        info.arch = { "x86_64" };
        info.packageInfoV2Module = module;
        nlohmann::json j = info;
        std::ofstream{ layerDir / "info.json" } << j.dump();

        LayerDir ld(layerDir);
        if (!ld.valid()) {
            ADD_FAILURE() << "layer dir should be valid";
            return std::nullopt;
        }
        auto imported = repo->importLayerDir(ld);
        if (!imported) {
            ADD_FAILURE() << imported.error().message();
            return std::nullopt;
        }

        auto ref = Reference::parse("stable:" + id + "/" + version + "/x86_64");
        if (!ref) {
            ADD_FAILURE() << ref.error().message();
            return std::nullopt;
        }
        return *ref;
    }

    std::unique_ptr<TempDir> tempDir;
    fs::path repoRoot;
    std::unique_ptr<OSTreeRepo> repo;
    std::optional<Reference> appRef;
    std::optional<Reference> runtimeRef;
};

TEST_F(RealRepoTest, ImportCommitsAndQueries)
{
    // Both committed layers show up in the cache.
    auto all = repo->listLayerItem();
    ASSERT_TRUE(all.has_value()) << all.error().message();
    ASSERT_EQ(all->size(), 2);

    auto local = repo->listLocal();
    ASSERT_TRUE(local.has_value()) << local.error().message();
    EXPECT_EQ(local->size(), 2);

    // getLayerItem by reference works for the app.
    auto appItem = repo->getLayerItem(*appRef, "binary");
    ASSERT_TRUE(appItem.has_value()) << appItem.error().message();
    EXPECT_EQ(appItem->info.id, "com.example.app");

    // getLayerDir by reference returns an existing directory.
    auto dir = repo->getLayerDir(*appRef, "binary", std::nullopt);
    ASSERT_TRUE(dir.has_value()) << dir.error().message();
    EXPECT_TRUE(fs::exists(dir->path() / "info.json"));
    EXPECT_TRUE(fs::exists(dir->path() / "files" / "usr" / "bin" / "hello"));

    // getModuleList returns the binary module.
    auto modules = repo->getModuleList(*appRef);
    EXPECT_EQ(modules, std::vector<std::string>{ "binary" });

    // getLayerCreateTime works for a committed layer.
    auto createTime = repo->getLayerCreateTime(*appItem);
    EXPECT_TRUE(createTime.has_value());
}

TEST_F(RealRepoTest, MarkDeletedChangesQueryAndList)
{
    ASSERT_TRUE(repo->getLayerItem(*appRef, "binary").has_value());

    EXPECT_FALSE(repo->isMarkedDeleted(*appRef, "binary"));

    auto mark = repo->markDeleted(*appRef, true, "binary", std::nullopt);
    ASSERT_TRUE(mark.has_value()) << mark.error().message();
    EXPECT_TRUE(repo->isMarkedDeleted(*appRef, "binary"));

    // deleted items are hidden from listLocal.
    auto local = repo->listLocal();
    ASSERT_TRUE(local.has_value()) << local.error().message();
    EXPECT_EQ(local->size(), 1);
    EXPECT_EQ(local->front().id, "org.deepin.runtime");
}

TEST_F(RealRepoTest, RemoveDeletesLayerAndCacheEntry)
{
    auto appItem = repo->getLayerItem(*appRef, "binary");
    ASSERT_TRUE(appItem.has_value()) << appItem.error().message();
    auto commit = appItem->commit;

    auto removed = repo->remove(*appRef, "binary", std::nullopt);
    ASSERT_TRUE(removed.has_value()) << removed.error().message();

    EXPECT_FALSE(repo->getLayerItem(*appRef, "binary").has_value());
    // after removal the checkpoint layer directory is gone
    EXPECT_FALSE(fs::exists(repoRoot / "layers" / commit));
}

TEST_F(RealRepoTest, PruneAfterRemovalSucceeds)
{
    auto removed = repo->remove(*appRef, "binary", std::nullopt);
    ASSERT_TRUE(removed.has_value()) << removed.error().message();

    auto pruned = repo->prune();
    EXPECT_TRUE(pruned.has_value()) << pruned.error().message();
}

TEST_F(RealRepoTest, ClearReferenceAndLatestLocal)
{
    auto fuzzy = FuzzyReference::parse("com.example.app");
    ASSERT_TRUE(fuzzy.has_value());

    auto cleared = repo->clearReferenceLocal(*fuzzy, true);
    ASSERT_TRUE(cleared.has_value()) << cleared.error().message();
    EXPECT_EQ(cleared->id, "com.example.app");

    auto latest = repo->latestLocalReference(*fuzzy);
    ASSERT_TRUE(latest.has_value()) << latest.error().message();
    EXPECT_EQ(latest->version.toString(), "1.0.0");
}

TEST_F(RealRepoTest, ListLocalAppsReturnsLatestVersionPerApp)
{
    importLayer("com.example.app", "2.0.0", "app", "binary",
                { "files/usr/bin/hello2" });

    auto apps = repo->listLocalApps();
    ASSERT_TRUE(apps.has_value()) << apps.error().message();
    // Only the latest version of com.example.app remains; runtime layers are
    // filtered out by kind.
    ASSERT_EQ(apps->size(), 1);
    EXPECT_EQ(apps->at(0).id, "com.example.app");
    EXPECT_EQ(apps->at(0).version, "2.0.0");
}

TEST_F(RealRepoTest, ConfigHelpersAndSetConfig)
{
    EXPECT_EQ(repo->getConfig().defaultRepo, "stable");
    EXPECT_EQ(repo->getOrderedConfig().repos.size(), 1);

    auto byAlias = repo->getRepoByAlias("stable");
    ASSERT_TRUE(byAlias.has_value()) << byAlias.error().message();
    EXPECT_EQ(byAlias->url, "https://example.com/repo");
    EXPECT_FALSE(repo->getRepoByAlias("missing").has_value());

    RepoConfigV2 newCfg{
        .defaultRepo = "a",
        .repos = { Repo{ .name = "a", .priority = 2, .url = "http://a" },
                   Repo{ .name = "b", .priority = 1, .url = "http://b" } },
        .version = 2,
    };
    auto set = repo->setConfig(newCfg);
    ASSERT_TRUE(set.has_value()) << set.error().message();
    EXPECT_EQ(repo->getConfig().repos.size(), 2);
}

TEST_F(RealRepoTest, GetMergedModuleDirFallsBackToLayerDir)
{
    auto merged = repo->getMergedModuleDir(*appRef, true, std::nullopt);
    ASSERT_TRUE(merged.has_value()) << merged.error().message();
    EXPECT_TRUE(fs::exists(merged->path() / "info.json"));
}

TEST_F(RealRepoTest, ExportAndUnexportReference)
{
    // In a minimal container the shared export config may be absent; neither
    // export nor unexport may crash, and unexport must clean any symlinks that
    // point into the layer dir.
    repo->exportReference(*appRef);
    repo->unexportReference(*appRef);

    auto item = repo->getLayerItem(*appRef, "binary");
    ASSERT_TRUE(item.has_value()) << item.error().message();
    repo->unexportReference((repoRoot / "layers" / item->commit).string());
}

TEST_F(RealRepoTest, QueriesForMissingReferencesFail)
{
    auto missingRef = Reference::parse("stable:com.example.nope/1.0.0/x86_64");
    ASSERT_TRUE(missingRef.has_value());
    EXPECT_FALSE(repo->getLayerItem(*missingRef, "binary").has_value());
    EXPECT_FALSE(repo->getLayerDir(*missingRef, "binary", std::nullopt).has_value());

    auto fuzzy = FuzzyReference::parse("com.example.nope");
    ASSERT_TRUE(fuzzy.has_value());
    EXPECT_FALSE(repo->clearReferenceLocal(*fuzzy, true).has_value());
}

TEST_F(RealRepoTest, MergeModulesMergesMultipleModules)
{
    // Add a second (develop) module for the same app id/version/arch.
    auto developRef = importLayer("com.example.app",
                                  "1.0.0",
                                  "app",
                                  "develop",
                                  { "files/usr/include/hello.h" });
    ASSERT_TRUE(developRef.has_value());

    auto modules = repo->getModuleList(*appRef);
    EXPECT_EQ(modules, (std::vector<std::string>{ "binary", "develop" }));

    auto merged = repo->mergeModules();
    ASSERT_TRUE(merged.has_value()) << merged.error().message();

    // getMergedModuleDir must now resolve the merged directory.
    auto resolved = repo->getMergedModuleDir(*appRef, false, std::nullopt);
    ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
    EXPECT_TRUE(fs::is_directory(resolved->path()));
}

TEST_F(RealRepoTest, CreateTempMergedModuleDirWorks)
{
    auto developRef = importLayer("com.example.app",
                                  "1.0.0",
                                  "app",
                                  "develop",
                                  { "files/usr/include/hello.h" });
    ASSERT_TRUE(developRef.has_value());

    // The caller must ensure the <repo>/merged output directory exists.
    ASSERT_TRUE(fs::create_directories(repoRoot / "merged"));

    auto tmp = repo->createTempMergedModuleDir(*appRef, { "binary", "develop" });
    ASSERT_TRUE(tmp.has_value()) << tmp.error().message();
    EXPECT_TRUE(fs::is_directory(tmp->path()));

    // missing module reports an error
    auto missing = repo->createTempMergedModuleDir(*appRef, { "binary", "develop", "nope" });
    EXPECT_FALSE(missing.has_value());
    // unknown ref reports an error
    auto missingRef = Reference::parse("stable:com.example.gone/1.0.0/x86_64");
    ASSERT_TRUE(missingRef.has_value());
    EXPECT_FALSE(repo->createTempMergedModuleDir(*missingRef, { "binary" }).has_value());

    std::error_code ec;
    fs::remove_all(tmp->path(), ec);
}

TEST_F(RealRepoTest, ListLocalByQuery)
{
    auto all = repo->listLocalBy(repoCacheQuery{ .id = "com.example.app" });
    ASSERT_TRUE(all.has_value()) << all.error().message();
    ASSERT_EQ(all->size(), 1);
    EXPECT_EQ(all->front().info.id, "com.example.app");

    auto runtime = repo->listLocalBy(repoCacheQuery{ .id = "org.deepin.runtime" });
    ASSERT_TRUE(runtime.has_value()) << runtime.error().message();
    ASSERT_EQ(runtime->size(), 1);
    EXPECT_EQ(runtime->front().info.id, "org.deepin.runtime");
}

// Understandable: upgradableApps needs the remote side, which is mocked while
// the local side runs against a real repo containing a committed app layer.
class RealRepoUpgradeMock : public linglong::repo::OSTreeRepo
{
public:
    RealRepoUpgradeMock(const fs::path &path, RepoConfigV2 cfg)
        : OSTreeRepo(path, std::move(cfg))
    {
    }

    using linglong::repo::OSTreeRepo::init;

    MOCK_METHOD(utils::error::Result<linglong::package::ReferenceWithRepo>,
                latestRemoteReference,
                (const FuzzyReference &fuzzyRef),
                (override, const, noexcept));
};

TEST_F(RealRepoTest, UpgradableAppsDetectsNewerRemote)
{
    // The repo already contains com.example.app/1.0.0 from SetUp. Re-open it
    // through the mock subclass so the remote lookup can be stubbed.
    auto mock = std::make_unique<RealRepoUpgradeMock>(repoRoot, makeRepoConfig());
    auto init = mock->init(false);
    ASSERT_TRUE(init.has_value()) << init.error().message();

    auto remoteRef = Reference::parse("stable:com.example.app/2.0.0/x86_64");
    ASSERT_TRUE(remoteRef.has_value());
    EXPECT_CALL(*mock, latestRemoteReference(::testing::_))
      .WillOnce(::testing::Return(
        linglong::package::ReferenceWithRepo{
          .repo = linglong::api::types::v1::Repo{
            .name = "stable", .priority = 0, .url = "https://example.com/repo" },
          .reference = *remoteRef,
        }));

    auto upgradeList = mock->upgradableApps();
    ASSERT_TRUE(upgradeList.has_value()) << upgradeList.error().message();
    ASSERT_EQ(upgradeList->size(), 1);
    EXPECT_EQ(upgradeList->at(0).first.version.toString(), "1.0.0");
    EXPECT_EQ(upgradeList->at(0).second.reference.version.toString(), "2.0.0");
}

} // namespace

} // namespace linglong::repo::test
