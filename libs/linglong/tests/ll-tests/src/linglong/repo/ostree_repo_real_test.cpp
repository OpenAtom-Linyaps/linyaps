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
#include "linglong/package_manager/task.h"
#include "linglong/repo/ostree_repo.h"

#include <algorithm>
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
        repoRoot = tempDir->path() / "repo-root";
        ASSERT_TRUE(fs::create_directories(repoRoot));

        auto created = OSTreeRepo::create(repoRoot, makeRepoConfig());
        ASSERT_TRUE(created.has_value()) << created.error().message();
        repo = std::move(*created);

        appRef = importLayer(
          "com.example.app",
          "1.0.0",
          "app",
          "binary",
          { "files/usr/bin/hello", "files/share/applications/com.example.app.desktop" });
        ASSERT_TRUE(appRef.has_value()) << "failed to import app layer";
        runtimeRef = importLayer(
          "org.deepin.runtime",
          "23",
          "runtime",
          "binary",
          { "files/usr/lib/libruntime.so", "files/lib/systemd/user/org.deepin.runtime.service" });
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
    auto dir = repo->getLayerDir(*appRef, "binary");
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

    auto mark = repo->markDeleted(*appRef, true, "binary");
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

    auto removed = repo->remove(*appRef, "binary");
    ASSERT_TRUE(removed.has_value()) << removed.error().message();

    EXPECT_FALSE(repo->getLayerItem(*appRef, "binary").has_value());
    // after removal the checkpoint layer directory is gone
    EXPECT_FALSE(fs::exists(repoRoot / "layers" / commit));
}

TEST_F(RealRepoTest, PruneAfterRemovalSucceeds)
{
    auto removed = repo->remove(*appRef, "binary");
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
    importLayer("com.example.app", "2.0.0", "app", "binary", { "files/usr/bin/hello2" });

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

TEST_F(RealRepoTest, SetConfigPreservesDeletedLayersAcrossReload)
{
    ASSERT_TRUE(repo->markDeleted(*appRef, true).has_value());
    auto before = repo->listLocalBy({});
    ASSERT_TRUE(before.has_value());
    auto expected = nlohmann::json(*before);

    auto config = repo->getConfig();
    config.repos.front().url = "https://example.com/updated";
    ASSERT_TRUE(repo->setConfig(config).has_value());

    auto checkLayers = [&]() {
        auto items = repo->listLocalBy({});
        ASSERT_TRUE(items.has_value());
        EXPECT_EQ(nlohmann::json(*items), expected);
        EXPECT_TRUE(repo->isMarkedDeleted(*appRef, "binary"));
        auto visible = repo->listLocalBy(repoCacheQuery{ .deleted = false });
        ASSERT_TRUE(visible.has_value());
        ASSERT_EQ(visible->size(), 1);
        EXPECT_EQ(visible->front().info.id, runtimeRef->id);
        EXPECT_EQ(nlohmann::json(repo->getConfig()), nlohmann::json(config));
    };
    checkLayers();

    repo.reset();
    auto reopened = OSTreeRepo::loadFromPath(repoRoot);
    ASSERT_TRUE(reopened.has_value());
    repo = std::move(*reopened);
    checkLayers();
}

TEST_F(RealRepoTest, GetMergedModuleDirFallsBackToLayerDir)
{
    auto merged = repo->getMergedModuleDir(*appRef, true);
    ASSERT_TRUE(merged.has_value()) << merged.error().message();
    EXPECT_TRUE(fs::exists(merged->path() / "info.json"));
}

TEST_F(RealRepoTest, ExportAndUnexportReference)
{
    // In a minimal container the shared export config may be absent; neither
    // export nor unexport may crash, and unexport must clean any symlinks that
    // point into the layer dir.
    repo->exportAppReference(*appRef);
    repo->unexportAppReference(*appRef);

    auto item = repo->getLayerItem(*appRef, "binary");
    ASSERT_TRUE(item.has_value()) << item.error().message();
    repo->unexportLayerSignData(*item);
}

TEST_F(RealRepoTest, QueriesForMissingReferencesFail)
{
    auto missingRef = Reference::parse("stable:com.example.nope/1.0.0/x86_64");
    ASSERT_TRUE(missingRef.has_value());
    EXPECT_FALSE(repo->getLayerItem(*missingRef, "binary").has_value());
    EXPECT_FALSE(repo->getLayerDir(*missingRef, "binary").has_value());

    auto fuzzy = FuzzyReference::parse("com.example.nope");
    ASSERT_TRUE(fuzzy.has_value());
    EXPECT_FALSE(repo->clearReferenceLocal(*fuzzy, true).has_value());
}

TEST_F(RealRepoTest, MergeModulesMergesMultipleModules)
{
    // Add a second (develop) module for the same app id/version/arch.
    auto developRef =
      importLayer("com.example.app", "1.0.0", "app", "develop", { "files/usr/include/hello.h" });
    ASSERT_TRUE(developRef.has_value());

    auto modules = repo->getModuleList(*appRef);
    EXPECT_EQ(modules, (std::vector<std::string>{ "binary", "develop" }));

    auto merged = repo->mergeModules();
    ASSERT_TRUE(merged.has_value()) << merged.error().message();

    // getMergedModuleDir must now resolve the merged directory.
    auto resolved = repo->getMergedModuleDir(*appRef, false);
    ASSERT_TRUE(resolved.has_value()) << resolved.error().message();
    EXPECT_TRUE(fs::is_directory(resolved->path()));
}

TEST_F(RealRepoTest, CreateTempMergedModuleDirWorks)
{
    auto developRef =
      importLayer("com.example.app", "1.0.0", "app", "develop", { "files/usr/include/hello.h" });
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
      .WillOnce(::testing::Return(linglong::package::ReferenceWithRepo{
        .repo = linglong::api::types::v1::Repo{ .name = "stable",
                                                .priority = 0,
                                                .url = "https://example.com/repo" },
        .reference = *remoteRef,
      }));

    auto upgradeList = mock->upgradableApps();
    ASSERT_TRUE(upgradeList.has_value()) << upgradeList.error().message();
    ASSERT_EQ(upgradeList->size(), 1);
    EXPECT_EQ(upgradeList->at(0).first.version.toString(), "1.0.0");
    EXPECT_EQ(upgradeList->at(0).second.reference.version.toString(), "2.0.0");
}

class LocalRemoteRepoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        sourceRoot = tempDir->path() / "source";
        remoteRoot = tempDir->path() / "remote";
        consumerRoot = tempDir->path() / "consumer";
        ASSERT_TRUE(fs::create_directories(sourceRoot));

        auto sourceConfig = makeRepoConfig();
        auto created = OSTreeRepo::create(sourceRoot, sourceConfig);
        ASSERT_TRUE(created.has_value()) << created.error().message();
        source = std::move(*created);

        appRef =
          importLayer("org.example.remote",
                      "2.0.0",
                      "app",
                      "binary",
                      { "files/usr/bin/remote-app", "files/share/applications/remote.desktop" });
        ASSERT_TRUE(appRef.has_value());
        legacyRuntimeRef =
          importLayer("org.example.remote",
                      "2.0.0",
                      "app",
                      "runtime",
                      { "files/usr/bin/remote-app", "files/share/applications/remote.desktop" });
        ASSERT_TRUE(legacyRuntimeRef.has_value());
    }

    std::optional<Reference> importLayer(const std::string &id,
                                         const std::string &version,
                                         const std::string &kind,
                                         const std::string &module,
                                         const std::vector<std::string> &relFiles)
    {
        auto layerDir = sourceRoot / ("layersrc-" + id + "-" + module);
        std::error_code ec;
        fs::remove_all(layerDir, ec);
        ec.clear();
        for (const auto &rel : relFiles) {
            auto path = layerDir / rel;
            fs::create_directories(path.parent_path(), ec);
            if (ec) {
                ADD_FAILURE() << "create dirs failed: " << ec.message();
                return std::nullopt;
            }
            std::ofstream{ path } << "remote content";
        }

        PackageInfoV2 info;
        info.id = id;
        info.version = version;
        info.kind = kind;
        info.channel = "stable";
        info.arch = { "x86_64" };
        info.packageInfoV2Module = module;
        std::ofstream{ layerDir / "info.json" } << nlohmann::json(info).dump();

        auto imported = source->importLayerDir(LayerDir{ layerDir });
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

    void publishRemote(const std::vector<std::pair<std::string, std::string>> &refs = {})
    {
        const auto remoteRepo = remoteRoot / "repos" / "stable";
        std::error_code ec;
        fs::create_directories(remoteRepo.parent_path(), ec);
        ASSERT_FALSE(ec) << ec.message();
        fs::copy(sourceRoot / "repo", remoteRepo, fs::copy_options::recursive, ec);
        ASSERT_FALSE(ec) << ec.message();

        const auto localRefs = remoteRepo / "refs" / "remotes" / "local";
        ASSERT_TRUE(fs::exists(localRefs));
        fs::copy(localRefs,
                 remoteRepo / "refs" / "heads",
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                 ec);
        ASSERT_FALSE(ec) << ec.message();

        for (const auto &[from, to] : refs) {
            const auto sourceRef = remoteRepo / "refs" / "heads" / from;
            ASSERT_TRUE(fs::exists(sourceRef)) << sourceRef;
            fs::create_directories((remoteRepo / "refs" / "heads" / to).parent_path(), ec);
            ASSERT_FALSE(ec) << ec.message();
            fs::copy_file(sourceRef, remoteRepo / "refs" / "heads" / to, ec);
            ASSERT_FALSE(ec) << ec.message();
        }

        auto consumerConfig = makeRepoConfig();
        consumerConfig.repos.front().url = "file://" + remoteRoot.string();
        fs::create_directories(consumerRoot, ec);
        ASSERT_FALSE(ec) << ec.message();
        auto loaded = OSTreeRepo::create(consumerRoot, consumerConfig);
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        consumer = std::move(*loaded);
        remoteRepoConfig = consumerConfig.repos.front();
    }

    void regenerateRemoteSummary(const fs::path &repoPath)
    {
        g_autoptr(GError) error = nullptr;
        g_autoptr(GFile) repoFile = g_file_new_for_path(repoPath.c_str());
        g_autoptr(OstreeRepo) repo = ostree_repo_new(repoFile);
        ASSERT_TRUE(ostree_repo_open(repo, nullptr, &error))
          << (error != nullptr ? error->message : "failed to open remote ostree repo");
        ASSERT_TRUE(ostree_repo_regenerate_summary(repo, nullptr, nullptr, &error))
          << (error != nullptr ? error->message : "failed to regenerate remote summary");
    }

    package::ReferenceWithRepo remoteReference() const
    {
        return package::ReferenceWithRepo{ .repo = remoteRepoConfig, .reference = *appRef };
    }

    std::unique_ptr<TempDir> tempDir;
    fs::path sourceRoot;
    fs::path remoteRoot;
    fs::path consumerRoot;
    std::unique_ptr<OSTreeRepo> source;
    std::unique_ptr<OSTreeRepo> consumer;
    std::optional<Reference> appRef;
    std::optional<Reference> legacyRuntimeRef;
    Repo remoteRepoConfig;
};

TEST_F(LocalRemoteRepoTest, PullImportsLayerAndFetchesMetadata)
{
    publishRemote();
    const auto refRepo = remoteReference();

    service::Task pullTask;
    auto pulled = consumer->pull(pullTask, refRepo, "binary");
    ASSERT_TRUE(pulled) << pulled.error().message();

    auto local = consumer->listLocal();
    ASSERT_TRUE(local) << local.error().message();
    ASSERT_EQ(local->size(), 1);
    EXPECT_EQ(local->front().id, "org.example.remote");

    auto item = consumer->getLayerItem(*appRef, "binary");
    ASSERT_TRUE(item) << item.error().message();
    EXPECT_EQ(item->info.version, "2.0.0");
    EXPECT_TRUE(
      fs::exists(consumerRoot / "layers" / item->commit / "files" / "usr" / "bin" / "remote-app"));

    auto metadata = consumer->fetchRefMetaData(refRepo, "binary", true);
    ASSERT_TRUE(metadata) << metadata.error().message();
    EXPECT_FALSE(metadata->getRev().empty());
    auto metadataInfo = metadata->getPackageInfo();
    ASSERT_TRUE(metadataInfo) << metadataInfo.error().message();
    EXPECT_EQ(metadataInfo->id, "org.example.remote");

    auto remoteInfo = consumer->fetchRemotePackageInfo(refRepo, "binary");
    ASSERT_TRUE(remoteInfo) << remoteInfo.error().message();
    EXPECT_EQ(remoteInfo->packageInfoV2Module, "binary");
}

TEST_F(LocalRemoteRepoTest, GetRefStatisticsWithoutObjectSizeMetadataReturnsError)
{
    publishRemote();
    auto metadata = consumer->fetchRefMetaData(remoteReference(), "binary", true);
    ASSERT_TRUE(metadata) << metadata.error().message();

    auto statistics = consumer->getRefStatistics(*metadata);
    if (statistics.has_value()) {
        GTEST_SKIP() << "OSTree commit exposes object size metadata; this test covers the "
                        "missing-metadata error path";
    }

    EXPECT_THAT(statistics.error().message(),
                ::testing::HasSubstr("ostree_commit_get_object_sizes"))
      << "OSTree version or commit metadata changed; update the precondition if this is no "
         "longer the missing-object-size error";
}

TEST_F(LocalRemoteRepoTest, PullFallsBackToLegacyRuntimeBranch)
{
    const std::string v2Ref = "stable/org.example.remote/2.0.0/x86_64/binary";
    const std::string legacyRef = "stable/org.example.remote/2.0.0/x86_64/runtime";
    publishRemote();

    const auto remoteHeads = remoteRoot / "repos" / "stable" / "refs" / "heads";
    std::error_code ec;
    ASSERT_TRUE(fs::exists(remoteHeads / legacyRef));
    ASSERT_TRUE(fs::remove(remoteHeads / v2Ref, ec)) << ec.message();
    EXPECT_FALSE(fs::exists(remoteHeads / v2Ref))
      << "binary candidate must be absent so pull has to fall back";
    regenerateRemoteSummary(remoteRoot / "repos" / "stable");

    service::Task pullTask;
    auto pulled = consumer->pull(pullTask, remoteReference(), "binary");

    ASSERT_TRUE(pulled) << pulled.error().message();
    EXPECT_FALSE(fs::exists(remoteHeads / v2Ref));
    auto item = consumer->getLayerItem(*appRef, "runtime");
    ASSERT_TRUE(item) << item.error().message();
    EXPECT_EQ(item->info.packageInfoV2Module, "runtime")
      << "fallback must install the layer published on the legacy runtime branch";
}

TEST_F(LocalRemoteRepoTest, PullAndMetadataReportMissingRemoteBranch)
{
    publishRemote();
    auto missingRef = Reference::parse("stable:org.example.remote/9.9.9/x86_64");
    ASSERT_TRUE(missingRef.has_value());
    const auto missing =
      package::ReferenceWithRepo{ .repo = remoteRepoConfig, .reference = *missingRef };

    service::Task pullTask;
    auto pulled = consumer->pull(pullTask, missing, "binary");
    EXPECT_FALSE(pulled.has_value());
    EXPECT_THAT(pulled.error().message(), ::testing::HasSubstr("ostree_repo_pull_with_options"));

    auto metadata = consumer->fetchRefMetaData(missing, "binary", false);
    EXPECT_FALSE(metadata.has_value());
}

TEST(RefMetaDataTest, GetPackageInfoWhenEmptyReturnsError)
{
    linglong::repo::RefMetaData meta{ "rev" };

    auto result = meta.getPackageInfo();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("package info is not provided"));
}

TEST(RefMetaDataTest, GetPackageInfoParsesValidV2Json)
{
    const std::string content = R"({
        "arch": ["x86_64"],
        "base": "org.deepin.base",
        "channel": "main",
        "command": ["testapp"],
        "description": "Test application",
        "id": "com.example.testapp",
        "kind": "app",
        "module": "binary",
        "name": "TestApp",
        "schema_version": "1.0",
        "size": 1024,
        "version": "1.0.0"
    })";
    linglong::repo::RefMetaData meta{ "rev", content };

    auto result = meta.getPackageInfo();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->id, "com.example.testapp");
    EXPECT_EQ(result->version, "1.0.0");
    EXPECT_EQ(result->packageInfoV2Module, "binary");
}

TEST(RefMetaDataTest, GetPackageInfoRejectsInvalidContent)
{
    linglong::repo::RefMetaData meta{ "rev", "this is not valid json" };

    auto result = meta.getPackageInfo();

    EXPECT_FALSE(result.has_value());
}

namespace {

using linglong::api::types::v1::RepositoryCacheLayersItem;

// Exposes the protected entry-export helpers so their behaviour can be verified
// against a real OSTree repository and an isolated entries tree.
class ExportRepo : public OSTreeRepo
{
public:
    ExportRepo(const std::filesystem::path &root, RepoConfigV2 config)
        : OSTreeRepo(root, std::move(config))
    {
        auto initialized = this->init(true);
        EXPECT_TRUE(initialized.has_value()) << initialized.error().message();
    }

    using OSTreeRepo::buildPullRefCandidates;
    using OSTreeRepo::exportAppBinaries;
    using OSTreeRepo::exportAppEntries;
    using OSTreeRepo::exportDir;
    using OSTreeRepo::exportLayerEntries;
    using OSTreeRepo::exportLayerSignData;
    using OSTreeRepo::getDefaultSharedDir;
    using OSTreeRepo::getEntriesDir;
    using OSTreeRepo::getOverlayShareDir;
    using OSTreeRepo::shouldFallbackToRuntimeBranch;

    utils::error::Result<bool> shouldExportSignData() const noexcept override { return true; }
};

class RealRepoExportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        repoRoot = tempDir->path() / "repo-root";
        ASSERT_TRUE(std::filesystem::create_directories(repoRoot));

        auto config = makeRepoConfig();
        config.repos.push_back(Repo{ .name = "testing", .priority = 5, .url = "https://test" });
        config.repos.push_back(Repo{ .name = "stable2", .priority = 0, .url = "https://two" });
        repo = std::make_unique<ExportRepo>(repoRoot, config);
    }

    std::optional<RepositoryCacheLayersItem>
    importLayer(const std::string &id,
                const std::string &kind,
                const std::string &module,
                const std::vector<std::string> &files,
                const std::vector<std::string> &command = {})
    {
        auto layerDir = tempDir->path() / ("export-" + id + "-" + module);
        std::error_code ec;
        std::filesystem::remove_all(layerDir, ec);
        std::filesystem::create_directories(layerDir / "files", ec);
        for (const auto &rel : files) {
            auto path = layerDir / rel;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream{ path } << "[Desktop Entry]\nName=Demo\nExec=/usr/bin/demo %U\n"
                                     "Type=Application\n";
        }

        PackageInfoV2 info;
        info.id = id;
        info.version = "1.0.0";
        info.kind = kind;
        info.channel = "stable";
        info.arch = { "x86_64" };
        info.packageInfoV2Module = module;
        info.command = command;
        std::ofstream{ layerDir / "info.json" } << nlohmann::json(info).dump();

        auto imported = repo->importLayerDir(LayerDir{ layerDir });
        if (!imported) {
            ADD_FAILURE() << imported.error().message();
            return std::nullopt;
        }
        return *imported;
    }

    std::unique_ptr<TempDir> tempDir;
    std::filesystem::path repoRoot;
    std::unique_ptr<ExportRepo> repo;
};

TEST_F(RealRepoExportTest, ExportDirLinksFilesAndRecursesIntoDirectories)
{
    const auto source = tempDir->path() / "source";
    std::filesystem::create_directories(source / "nested");
    std::ofstream{ source / "plain.txt" } << "plain";
    std::ofstream{ source / "nested" / "deep.txt" } << "deep";
    std::ofstream{ source / "backup.linyaps.original" } << "backup";

    const auto entries = tempDir->path() / "entries";
    auto exported = repo->exportDir("com.example.app", source, entries, "files", 10);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();

    EXPECT_TRUE(std::filesystem::is_symlink(entries / "files" / "plain.txt"));
    EXPECT_TRUE(std::filesystem::is_symlink(entries / "files" / "nested" / "deep.txt"));
    EXPECT_FALSE(std::filesystem::exists(entries / "files" / "backup.linyaps.original"));
    EXPECT_EQ(std::filesystem::canonical(entries / "files" / "plain.txt"), source / "plain.txt");
}

TEST_F(RealRepoExportTest, ExportDirRejectsInvalidDestinationsAndSources)
{
    const auto source = tempDir->path() / "source";
    std::filesystem::create_directories(source);
    const auto entries = tempDir->path() / "entries";

    auto absolute = repo->exportDir("com.example.app", source, entries, "/absolute", 10);
    EXPECT_FALSE(absolute.has_value());

    auto parent = repo->exportDir("com.example.app", source, entries, "../escape", 10);
    EXPECT_FALSE(parent.has_value());

    auto missing = repo->exportDir("com.example.app", source / "missing", entries, "files", 10);
    EXPECT_FALSE(missing.has_value());
}

TEST_F(RealRepoExportTest, ExportDirStopsAtZeroDepth)
{
    const auto source = tempDir->path() / "source";
    std::filesystem::create_directories(source);
    std::ofstream{ source / "plain.txt" } << "plain";
    const auto entries = tempDir->path() / "entries";

    auto exported = repo->exportDir("com.example.app", source, entries, "files", 0);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();
    EXPECT_FALSE(std::filesystem::exists(entries / "files"));
}

TEST_F(RealRepoExportTest, ExportDirRewritesApplicationEntriesInPlace)
{
    const auto source = tempDir->path() / "source";
    std::filesystem::create_directories(source / "share" / "applications");
    const auto desktop = source / "share" / "applications" / "com.example.app.desktop";
    std::ofstream{ desktop } << "[Desktop Entry]\nName=Demo\nExec=/usr/bin/demo %U\n"
                                "TryExec=/usr/bin/demo\nType=Application\n";

    const auto entries = tempDir->path() / "entries";
    auto exported = repo->exportDir("com.example.app", source / "share", entries, "share", 10);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();

    EXPECT_TRUE(std::filesystem::exists(desktop));
    EXPECT_TRUE(std::filesystem::exists(desktop.string() + ".linyaps.original"));
    EXPECT_TRUE(
      std::filesystem::exists(entries / "share" / "applications" / "com.example.app.desktop"));
}

TEST_F(RealRepoExportTest, ExportAppBinariesCreatesExecutableWrapperAndSkipsOtherModules)
{
    RepositoryCacheLayersItem item{};
    item.info.id = "com.example.app";
    item.info.packageInfoV2Module = "binary";
    item.info.command = std::vector<std::string>{ "/usr/bin/demo", "--flag" };

    const auto entries = tempDir->path() / "entries";
    auto exported = repo->exportAppBinaries(entries, item);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();

    const auto wrapper = entries / "bin" / "apps" / item.info.id / "bin" / item.info.id;
    ASSERT_TRUE(std::filesystem::exists(wrapper));
    EXPECT_TRUE(std::filesystem::is_symlink(entries / "bin" / item.info.id));
    EXPECT_EQ(std::filesystem::status(wrapper).permissions() & std::filesystem::perms::owner_exec,
              std::filesystem::perms::owner_exec);
    std::ifstream script{ wrapper };
    std::string content{ std::istreambuf_iterator<char>{ script },
                         std::istreambuf_iterator<char>{} };
    EXPECT_NE(content.find("/usr/bin/demo"), std::string::npos);

    RepositoryCacheLayersItem develop = item;
    develop.info.packageInfoV2Module = "develop";
    EXPECT_TRUE(repo->exportAppBinaries(entries, develop).has_value());

    RepositoryCacheLayersItem empty = item;
    empty.info.command = std::vector<std::string>{};
    EXPECT_TRUE(repo->exportAppBinaries(entries, empty).has_value());
    EXPECT_FALSE(std::filesystem::exists(entries / "bin" / item.info.id));
}

TEST_F(RealRepoExportTest, ExportAppEntriesExportsWrapperForImportedLayer)
{
    auto item = importLayer("com.example.export",
                            "app",
                            "binary",
                            { "entries/share/applications/com.example.export.desktop" },
                            { "/usr/bin/export" });
    ASSERT_TRUE(item.has_value());

    const auto entries = tempDir->path() / "entries";
    auto exported = repo->exportAppEntries(entries, *item);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();
    EXPECT_TRUE(std::filesystem::exists(entries / "bin" / "com.example.export"));

    auto noEntries = importLayer("com.example.noentries", "app", "binary", {});
    ASSERT_TRUE(noEntries.has_value());
    EXPECT_TRUE(repo->exportAppEntries(entries, *noEntries).has_value());
}

TEST_F(RealRepoExportTest, ExportLayerSignDataCopiesVerifyEntries)
{
    auto item = importLayer("com.example.signed",
                            "app",
                            "binary",
                            { "entries/share/deepin-elf-verify/sign.json" },
                            { "/usr/bin/signed" });
    ASSERT_TRUE(item.has_value());

    const auto entries = tempDir->path() / "entries";
    auto exported = repo->exportLayerSignData(entries, *item);
    ASSERT_TRUE(exported.has_value()) << exported.error().message();
    EXPECT_TRUE(std::filesystem::exists(entries / "share" / "deepin-elf-verify" / item->commit
                                        / "sign.json"));

    RepositoryCacheLayersItem missing = *item;
    missing.commit = "missing-commit";
    EXPECT_FALSE(repo->exportLayerSignData(entries, missing).has_value());
}

TEST_F(RealRepoExportTest, PriorityGroupsAliasesAndFallbackCandidates)
{
    auto groups = repo->getPriorityGroupedRepos();
    ASSERT_EQ(groups.size(), 2);
    auto testingGroup = std::find_if(groups.begin(), groups.end(), [](const auto &group) {
        return group.size() == 1 && group.front().name == "testing";
    });
    ASSERT_NE(testingGroup, groups.end());

    auto alias = repo->getRepoByAlias("stable2");
    ASSERT_TRUE(alias.has_value()) << alias.error().message();
    EXPECT_EQ(alias->name, "stable2");
    EXPECT_FALSE(repo->getRepoByAlias("missing").has_value());

    const auto binaryRef = Reference::parse("stable:com.example.app/1.0.0/x86_64");
    ASSERT_TRUE(binaryRef.has_value()) << binaryRef.error().message();
    EXPECT_EQ(repo->buildPullRefCandidates(*binaryRef, "binary").size(), 2);
    EXPECT_EQ(repo->buildPullRefCandidates(*binaryRef, "develop").size(), 1);

    EXPECT_FALSE(repo->shouldFallbackToRuntimeBranch("develop", nullptr));
    EXPECT_FALSE(repo->shouldFallbackToRuntimeBranch("binary", nullptr));
    GError noBranch{ G_FILE_ERROR, G_FILE_ERROR_FAILED, nullptr };
    noBranch.message = const_cast<gchar *>("No such branch");
    EXPECT_TRUE(repo->shouldFallbackToRuntimeBranch("binary", &noBranch));
    GError otherError{ G_FILE_ERROR, G_FILE_ERROR_FAILED, nullptr };
    otherError.message = const_cast<gchar *>("other");
    EXPECT_FALSE(repo->shouldFallbackToRuntimeBranch("binary", &otherError));
}

} // namespace

} // namespace

} // namespace linglong::repo::test
