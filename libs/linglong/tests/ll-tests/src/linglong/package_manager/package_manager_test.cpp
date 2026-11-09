/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/scoped_runtime_paths.h"
#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/common/dir.h"
#include "linglong/common/serialize/json.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <QDBusUnixFileDescriptor>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace v1 = api::types::v1;

utils::error::Result<void> ok() noexcept
{
    return utils::error::Result<void>{};
}

api::types::v1::RepoConfigV2 makeConfig()
{
    return api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .name = "stable", .url = "https://example.com/repo" } },
        .version = 2,
    };
}

package::Reference makeRef(const std::string &version = "1.0.0")
{
    return package::Reference::parse("main:org.example/" + version + "/x86_64").value();
}

std::string hostArch()
{
    return package::Architecture::currentCPUArchitecture().toString();
}

package::FuzzyReference makeFuzzyRef()
{
    return package::FuzzyReference::parse("main:org.example/1.0.0/x86_64").value();
}

package::ReferenceWithRepo makeRefWithRepo()
{
    return package::ReferenceWithRepo{ .repo = api::types::v1::Repo{ .name = "stable" },
                                       .reference = makeRef() };
}

package::Reference makeRefWithId(const std::string &id, const std::string &version = "1.0.0")
{
    return package::Reference::parse("main:" + id + "/" + version + "/x86_64").value();
}

package::ReferenceWithRepo makeRefWithRepoFor(const std::string &id)
{
    return package::ReferenceWithRepo{ .repo = api::types::v1::Repo{ .name = "stable" },
                                       .reference = makeRefWithId(id) };
}

api::types::v1::PackageInfoV2 makeAppInfo(const std::string &version = "1.0.0")
{
    return api::types::v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "org.example",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = version,
    };
}

api::types::v1::RepositoryCacheLayersItem makeLayerItem(const package::Reference &ref,
                                                        const std::string &kind)
{
    return api::types::v1::RepositoryCacheLayersItem{
        .commit = "commit-1",
        .deleted = std::nullopt,
        .info =
          api::types::v1::PackageInfoV2{
            .arch = std::vector<std::string>{ ref.arch.toString() },
            .channel = ref.channel,
            .id = ref.id,
            .kind = kind,
            .packageInfoV2Module = "binary",
            .version = ref.version.toString(),
          },
        .repo = "stable",
    };
}

long long replyCode(const QVariantMap &result)

{
    return result.value("code").toLongLong();
}

QString replyMessage(const QVariantMap &result)
{
    return result.value("message").toString();
}

// A repo collaborator that initializes a real ostree repository inside a temp
// dir, overrides the value-returning virtuals with injectable functions (same
// pattern as MockOstreeRepo) and keeps gmock verification for the
// void-returning virtuals (pull/merge/prune).
class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path, v1::RepoConfigV2 cfg)
        : repo::OSTreeRepo(path, std::move(cfg))
    {
    }

    bool initialize()
    {
        auto ret = this->init(true);
        return ret.has_value();
    }

    std::function<utils::error::Result<package::Reference>(const package::FuzzyReference &, bool)>
      onClearReferenceLocal;
    std::function<utils::error::Result<package::ReferenceWithRepo>(const package::FuzzyReference &)>
      onLatestRemoteReference;
    std::function<utils::error::Result<repo::RemotePackages>(const package::FuzzyReference &,
                                                             const std::optional<v1::Repo> &)>
      onMatchRemoteByPriority;
    std::function<std::vector<std::string>(const package::Reference &)> onGetModuleList;
    std::function<utils::error::Result<package::LayerDir>(const package::Reference &, bool)>
      onGetMergedModuleDir;
    std::function<utils::error::Result<v1::RepositoryCacheLayersItem>(const package::Reference &,
                                                                      std::string)>
      onGetLayerItem;
    std::function<utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>(
      const linglong::repo::repoCacheQuery &)>
      onListLocalBy;

    utils::error::Result<package::Reference> clearReferenceLocal(
      const package::FuzzyReference &fuzzyRef, bool semanticMatching) const noexcept override
    {
        LINGLONG_TRACE("clearReferenceLocal");
        if (onClearReferenceLocal) {
            return onClearReferenceLocal(fuzzyRef, semanticMatching);
        }
        return LINGLONG_ERR("not mocked");
    }

    utils::error::Result<package::ReferenceWithRepo>
    latestRemoteReference(const package::FuzzyReference &fuzzyRef) const noexcept override
    {
        LINGLONG_TRACE("latestRemoteReference");
        if (onLatestRemoteReference) {
            return onLatestRemoteReference(fuzzyRef);
        }
        return LINGLONG_ERR("not mocked");
    }

    utils::error::Result<repo::RemotePackages>
    matchRemoteByPriority(const package::FuzzyReference &fuzzyRef,
                          const std::optional<v1::Repo> &repo) const noexcept override
    {
        LINGLONG_TRACE("matchRemoteByPriority");
        if (onMatchRemoteByPriority) {
            return onMatchRemoteByPriority(fuzzyRef, repo);
        }
        return LINGLONG_ERR("not mocked");
    }

    std::vector<std::string> getModuleList(const package::Reference &ref) const noexcept override
    {
        LINGLONG_TRACE("getModuleList");
        if (onGetModuleList) {
            return onGetModuleList(ref);
        }
        return {};
    }

    utils::error::Result<package::LayerDir>
    getMergedModuleDir(const package::Reference &ref, bool fallbackLayerDir) const noexcept override
    {
        LINGLONG_TRACE("getMergedModuleDir");
        if (onGetMergedModuleDir) {
            return onGetMergedModuleDir(ref, fallbackLayerDir);
        }
        return LINGLONG_ERR("not mocked");
    }

    utils::error::Result<v1::RepositoryCacheLayersItem>
    getLayerItem(const package::Reference &ref, std::string module) const noexcept override
    {
        LINGLONG_TRACE("getLayerItem");
        if (onGetLayerItem) {
            return onGetLayerItem(ref, std::move(module));
        }
        return LINGLONG_ERR("not mocked");
    }

    utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>
    listLocalBy(const linglong::repo::repoCacheQuery &query) const noexcept override
    {
        LINGLONG_TRACE("listLocalBy");
        if (onListLocalBy) {
            return onListLocalBy(query);
        }
        return LINGLONG_ERR("not mocked");
    }

    MOCK_METHOD(utils::error::Result<void>,
                pull,
                (service::Task & taskContext,
                 const package::ReferenceWithRepo &refRepo,
                 const std::string &module),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>, mergeModules, (), (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>, prune, (), (override, noexcept));
};

class PackageManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        LINGLONG_TRACE("SetUp");
        tempDir = std::make_unique<TempDir>();
        repoDir = tempDir->path() / "repo";
        std::filesystem::create_directories(repoDir);

        config = makeConfig();
        repoOwner = std::make_unique<NiceMock<MockRepo>>(repoDir, config);
        ASSERT_TRUE(repoOwner->initialize());
        repo = repoOwner.get();

        pmOwner =
          std::make_unique<service::PackageManager>(std::move(repoOwner),
                                                    std::unique_ptr<runtime::ContainerBuilder>(),
                                                    nullptr);
        pm = pmOwner.get();

        // Value-returning repo collaborators fail closed by default; the
        // void-returning mocks use gmock's built-in default (engaged Result<void>).
    }

    std::unique_ptr<TempDir> tempDir;
    std::filesystem::path repoDir;
    v1::RepoConfigV2 config;
    std::unique_ptr<NiceMock<MockRepo>> repoOwner;
    NiceMock<MockRepo> *repo{ nullptr };
    std::unique_ptr<service::PackageManager> pmOwner;
    service::PackageManager *pm{ nullptr };
};

TEST_F(PackageManagerTest, GetConfiguration_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->getConfiguration();

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

TEST_F(PackageManagerTest, GetConfiguration_AfterDaemonModeInit_ReturnsSerializedRepoConfig)
{
    pm->initDaemonMode(true);

    auto result = pm->getConfiguration();

    EXPECT_EQ(result.value("defaultRepo").toString(), QString("stable"));
    ASSERT_FALSE(result.value("repos").toList().isEmpty());
}

TEST_F(PackageManagerTest, SetConfiguration_AppliesNewValidConfig)
{
    pm->initDaemonMode(true);

    v1::RepoConfigV2 newConfig{
        .defaultRepo = "edge",
        .repos = { api::types::v1::Repo{ .name = "stable", .url = "https://example.com/repo" },
                   api::types::v1::Repo{ .name = "edge", .url = "https://edge.example.com/repo" } },
        .version = 2,
    };

    pm->SetConfiguration(common::serialize::toQVariantMap(newConfig));

    EXPECT_EQ(repo->getConfig().defaultRepo, std::string("edge"));
}

TEST_F(PackageManagerTest, SetConfiguration_WhenConfigUnchanged_KeepsConfig)
{
    pm->initDaemonMode(true);

    pm->SetConfiguration(common::serialize::toQVariantMap(config));

    EXPECT_EQ(repo->getConfig().defaultRepo, std::string("stable"));
}

TEST_F(PackageManagerTest, Install_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->Install(QVariantMap{});

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

TEST_F(PackageManagerTest, Search_InvalidParameters_ReturnsError)
{
    pm->initDaemonMode(true);

    auto result = pm->Search(QVariantMap{});

    EXPECT_NE(replyCode(result), 0);
}

TEST_F(PackageManagerTest, InstallRefModule_WhenNotMarkedDeleted_PullsModule)
{
    LINGLONG_TRACE("InstallRefModule_WhenNotMarkedDeleted_PullsModule");
    service::Task taskPlaceholder;

    EXPECT_CALL(*repo, pull(_, _, "binary")).WillOnce(Return(ok()));

    auto result = pm->installRefModule(taskPlaceholder, makeRefWithRepo(), "binary");

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, InstallRefModule_WhenPullFails_ReturnsError)
{
    LINGLONG_TRACE("InstallRefModule_WhenPullFails_ReturnsError");
    service::Task taskPlaceholder;

    EXPECT_CALL(*repo, pull(_, _, "binary")).WillOnce(Return(LINGLONG_ERR("pull failed")));

    auto result = pm->installRefModule(taskPlaceholder, makeRefWithRepo(), "binary");

    EXPECT_FALSE(result.has_value());
}

TEST_F(PackageManagerTest, InstallRef_EmptyModules_ReturnsOkWithoutRepoCall)
{
    service::Task taskPlaceholder;

    EXPECT_CALL(*repo, pull(_, _, _)).Times(0);

    auto result = pm->installRef(taskPlaceholder, makeRefWithRepo(), {});

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, InstallRef_InstallsAllModules)
{
    service::Task taskPlaceholder;

    EXPECT_CALL(*repo, pull(_, _, "binary")).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, pull(_, _, "develop")).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));

    auto result = pm->installRef(taskPlaceholder, makeRefWithRepo(), { "binary", "develop" });

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, InstallRef_WhenPullFailsStopsAndReturnsError)
{
    LINGLONG_TRACE("InstallRef_WhenPullFailsStopsAndReturnsError");
    service::Task taskPlaceholder;

    EXPECT_CALL(*repo, pull(_, _, _))
      .WillOnce(Return(ok()))
      .WillOnce(Return(LINGLONG_ERR("pull failed")));

    auto result = pm->installRef(taskPlaceholder, makeRefWithRepo(), { "binary", "develop" });

    EXPECT_FALSE(result.has_value());
}

TEST_F(PackageManagerTest, InstallDependsRef_WhenAlreadyInstalled_ReturnsOk)
{
    service::Task taskPlaceholder;
    auto installed = makeRef();
    bool remoteQueried = false;
    repo->onClearReferenceLocal = [installed](const auto &, bool) {
        return utils::error::Result<package::Reference>{ installed };
    };
    repo->onLatestRemoteReference = [&remoteQueried](const auto &) {
        LINGLONG_TRACE("unexpected latestRemoteReference");
        remoteQueried = true;
        return LINGLONG_ERR("should not be reached");
    };
    EXPECT_CALL(*repo, pull(_, _, _)).Times(0);

    auto result = pm->installDependsRef(taskPlaceholder, "org.example");

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(remoteQueried);
}

TEST_F(PackageManagerTest, InstallDependsRef_WhenNotInstalled_InstallsRemoteRef)
{
    LINGLONG_TRACE("InstallDependsRef_WhenNotInstalled_InstallsRemoteRef");
    service::Task taskPlaceholder;
    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("not found");
    };
    repo->onLatestRemoteReference = [](const auto &) {
        return utils::error::Result<package::ReferenceWithRepo>{ makeRefWithRepo() };
    };
    EXPECT_CALL(*repo, pull(_, _, "binary")).WillOnce(Return(ok()));

    auto result = pm->installDependsRef(taskPlaceholder, "org.example");

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, InstallDependsRef_WhenRemoteLookupFails_ReturnsError)
{
    LINGLONG_TRACE("InstallDependsRef_WhenRemoteLookupFails_ReturnsError");
    service::Task taskPlaceholder;
    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("not found");
    };
    repo->onLatestRemoteReference = [](const auto &) {
        LINGLONG_TRACE("latestRemoteReference");
        return LINGLONG_ERR("remote lookup failed for org.example");
    };
    EXPECT_CALL(*repo, pull(_, _, _)).Times(0);

    auto result = pm->installDependsRef(taskPlaceholder, "org.example");

    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("remote lookup failed for org.example"));
}

TEST_F(PackageManagerTest, InstallDependsRef_InvalidRef_ReturnsError)
{
    service::Task taskPlaceholder;

    auto result =
      pm->installDependsRef(taskPlaceholder, "org.example/1.0.0/not-a-real-architecture");

    EXPECT_FALSE(result.has_value());
}

TEST_F(PackageManagerTest, InstallDependsRef_AppliesProvidedChannelWhenMissing)
{
    LINGLONG_TRACE("InstallDependsRef_AppliesProvidedChannelWhenMissing");
    service::Task taskPlaceholder;
    bool channelChecked = false;
    repo->onClearReferenceLocal = [&channelChecked](const package::FuzzyReference &fuzzyRef, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        channelChecked = true;
        EXPECT_EQ(fuzzyRef.channel, std::make_optional<std::string>("main"));
        return LINGLONG_ERR("not found");
    };
    repo->onLatestRemoteReference = [](const auto &) {
        LINGLONG_TRACE("latestRemoteReference");
        return LINGLONG_ERR("remote down");
    };

    auto result = pm->installDependsRef(taskPlaceholder, "org.example", "main");

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(channelChecked);
}

TEST_F(PackageManagerTest, NeedToInstall_WhenAlreadyInstalled_ReturnsNullopt)
{
    auto installed = makeRef();
    repo->onClearReferenceLocal = [installed](const auto &, bool) {
        return utils::error::Result<package::Reference>{ installed };
    };

    auto result = pm->needToInstall("org.example", std::nullopt);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

TEST_F(PackageManagerTest, NeedToInstall_WhenNotInstalled_ReturnsRemoteRef)
{
    LINGLONG_TRACE("NeedToInstall_WhenNotInstalled_ReturnsRemoteRef");
    auto remote = makeRefWithRepo();
    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("not found");
    };
    repo->onLatestRemoteReference = [remote](const auto &) {
        return utils::error::Result<package::ReferenceWithRepo>{ remote };
    };

    auto result = pm->needToInstall("org.example", std::nullopt);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(*result);
    EXPECT_EQ((*result)->reference.toString(), makeRef().toString());
}

TEST_F(PackageManagerTest, NeedToInstall_InvalidRef_ReturnsError)
{
    auto result = pm->needToInstall("org.example/1.0.0/not-a-real-architecture", std::nullopt);

    EXPECT_FALSE(result.has_value());
}

TEST_F(PackageManagerTest, NeedToUpgrade_WhenNoLocalAndNoInstallIfMissing_ReturnsNullopt)
{
    std::optional<package::Reference> local;
    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("not found");
    };

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, false);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

TEST_F(PackageManagerTest, NeedToUpgrade_WhenRemoteMatchFails_ReturnsError)
{
    LINGLONG_TRACE("NeedToUpgrade_WhenRemoteMatchFails_ReturnsError");
    std::optional<package::Reference> local = makeRef();
    bool moduleListQueried = false;
    repo->onGetModuleList = [&moduleListQueried](const auto &) {
        moduleListQueried = true;
        return std::vector<std::string>{};
    };
    repo->onMatchRemoteByPriority = [](const auto &, const auto &) {
        LINGLONG_TRACE("matchRemoteByPriority");
        return LINGLONG_ERR("remote match failed for org.example");
    };
    EXPECT_CALL(*repo, pull(_, _, _)).Times(0);

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, false);

    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("remote match failed for org.example"));
    EXPECT_FALSE(moduleListQueried);
}

TEST_F(PackageManagerTest, NeedToUpgrade_WhenLocalIsUpToDate_ReturnsNullopt)
{
    std::optional<package::Reference> local = makeRef("1.0.0");

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "stable" }, { makeAppInfo("0.9.0") });
    repo->onMatchRemoteByPriority = [&remote](const auto &, const auto &) {
        return utils::error::Result<repo::RemotePackages>{ std::move(remote) };
    };

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, false);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

TEST_F(PackageManagerTest, NeedToUpgrade_WhenMissingAndInstallRequested_ReturnsInstallPair)
{
    std::optional<package::Reference> local;

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "stable" }, { makeAppInfo("1.0.0") });
    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("not found");
    };
    repo->onMatchRemoteByPriority = [&remote](const auto &, const auto &) {
        return utils::error::Result<repo::RemotePackages>{ std::move(remote) };
    };

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, true);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(*result);
    EXPECT_THAT((**result).second, ElementsAre("binary"));
}

TEST_F(PackageManagerTest, NeedToUpgrade_WhenLocalIsOutdated_ReturnsUpgradeModules)
{
    std::optional<package::Reference> local = makeRef("0.9.0");

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "stable" }, { makeAppInfo("1.0.0") });
    repo->onGetModuleList = [](const auto &) {
        return std::vector<std::string>{ "binary", "develop" };
    };
    repo->onMatchRemoteByPriority = [&remote](const auto &, const auto &) {
        return utils::error::Result<repo::RemotePackages>{ std::move(remote) };
    };

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, false);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(*result);
    EXPECT_THAT((**result).second, ElementsAre("binary"));
}

TEST_F(PackageManagerTest, InstallAppDepends_BaseOnly_InstallsBase)
{
    service::Task taskPlaceholder;
    v1::PackageInfoV2 app = makeAppInfo();
    app.base = "org.base";

    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("base is not installed");
    };
    repo->onLatestRemoteReference = [](const package::FuzzyReference &fuzzyRef) {
        EXPECT_EQ(fuzzyRef.id, "org.base");
        return utils::error::Result<package::ReferenceWithRepo>{ makeRefWithRepoFor("org.base") };
    };

    std::vector<std::string> pulled;
    EXPECT_CALL(*repo, pull(_, _, "binary"))
      .Times(1)
      .WillOnce(Invoke(
        [&pulled](service::Task &, const package::ReferenceWithRepo &refRepo, const std::string &) {
            pulled.push_back(refRepo.reference.id);
            return ok();
        }));

    auto result = pm->installAppDepends(taskPlaceholder, app);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_THAT(pulled, ElementsAre("org.base"));
}

TEST_F(PackageManagerTest, InstallAppDepends_WithRuntime_InstallsBaseAndRuntime)
{
    service::Task taskPlaceholder;
    v1::PackageInfoV2 app = makeAppInfo();
    app.base = "org.base";
    app.runtime = "org.runtime";

    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("dependency is not installed");
    };
    repo->onLatestRemoteReference = [](const package::FuzzyReference &fuzzyRef)
      -> utils::error::Result<package::ReferenceWithRepo> {
        if (fuzzyRef.id == "org.base") {
            return makeRefWithRepoFor("org.base");
        }
        EXPECT_EQ(fuzzyRef.id, "org.runtime");
        return makeRefWithRepoFor("org.runtime");
    };

    std::vector<std::string> pulled;
    EXPECT_CALL(*repo, pull(_, _, "binary"))
      .Times(2)
      .WillRepeatedly(Invoke(
        [&pulled](service::Task &, const package::ReferenceWithRepo &refRepo, const std::string &) {
            pulled.push_back(refRepo.reference.id);
            return ok();
        }));

    auto result = pm->installAppDepends(taskPlaceholder, app);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_THAT(pulled, ElementsAre("org.base", "org.runtime"));
}

TEST_F(PackageManagerTest, InstallAppDepends_WhenRuntimeInstallFails_PropagatesError)
{
    service::Task taskPlaceholder;
    v1::PackageInfoV2 app = makeAppInfo();
    app.base = "org.base";
    app.runtime = "org.runtime";

    repo->onClearReferenceLocal = [](const auto &, bool) {
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("dependency is not installed");
    };
    repo->onLatestRemoteReference = [](const package::FuzzyReference &fuzzyRef)
      -> utils::error::Result<package::ReferenceWithRepo> {
        if (fuzzyRef.id == "org.base") {
            return makeRefWithRepoFor("org.base");
        }
        EXPECT_EQ(fuzzyRef.id, "org.runtime");
        return makeRefWithRepoFor("org.runtime");
    };

    std::vector<std::string> pulled;
    EXPECT_CALL(*repo, pull(_, _, "binary"))
      .Times(2)
      .WillOnce(Invoke(
        [&pulled](service::Task &, const package::ReferenceWithRepo &refRepo, const std::string &) {
            pulled.push_back(refRepo.reference.id);
            return ok();
        }))
      .WillOnce(Invoke([&pulled](service::Task &,
                                 const package::ReferenceWithRepo &refRepo,
                                 const std::string &) -> utils::error::Result<void> {
          LINGLONG_TRACE("pull runtime");
          pulled.push_back(refRepo.reference.id);
          return LINGLONG_ERR("runtime pull failed for org.runtime");
      }));

    auto result = pm->installAppDepends(taskPlaceholder, app);

    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("runtime pull failed for org.runtime"));
    EXPECT_THAT(pulled, ElementsAre("org.base", "org.runtime"));
}

TEST_F(PackageManagerTest, Uninstall_WhenGetLayerItemFails_ReturnsError)
{
    service::PackageTask taskPlaceholder([](service::Task &) { }, nullptr);
    repo->onGetLayerItem = [](const package::Reference &, std::string) {
        LINGLONG_TRACE("getLayerItem");
        return LINGLONG_ERR("layer lookup failed for org.example");
    };
    EXPECT_CALL(*repo, mergeModules()).Times(0);
    EXPECT_CALL(*repo, prune()).Times(0);

    auto result = pm->Uninstall(taskPlaceholder, makeRef(), "binary", false);

    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("layer lookup failed for org.example"));
}

TEST_F(PackageManagerTest, Uninstall_WhenUninstallingNonAppModule_Completes)
{
    service::PackageTask taskPlaceholder([](service::Task &) { }, nullptr);

    auto ref = makeRef();
    auto installed = makeLayerItem(ref, "base");
    int getLayerItemCalls = 0;
    repo->onGetLayerItem = [&getLayerItemCalls, installed](
                             const package::Reference &,
                             std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        LINGLONG_TRACE("getLayerItem");
        if (getLayerItemCalls++ == 0) {
            return utils::error::Result<v1::RepositoryCacheLayersItem>{ installed };
        }
        return LINGLONG_ERR("layer already removed");
    };
    repo->onGetModuleList = [](const auto &) {
        return std::vector<std::string>{ "binary", "develop" };
    };
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->Uninstall(taskPlaceholder, ref, "binary", false);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(taskPlaceholder.state(), linglong::api::types::v1::State::Succeed);
}

TEST_F(PackageManagerTest, InstallFromFile_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->InstallFromFile(QDBusUnixFileDescriptor{ -1 }, "layer", QVariantMap{});

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

TEST_F(PackageManagerTest, Prune_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->Prune();

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

struct ImportedLayer
{
    package::Reference reference;
    api::types::v1::RepositoryCacheLayersItem item;
};

std::optional<ImportedLayer> importLayer(repo::OSTreeRepo &targetRepo,
                                         const std::filesystem::path &workspace,
                                         const std::string &id,
                                         const std::string &version,
                                         const std::string &kind,
                                         const std::string &module,
                                         const std::optional<std::string> &base = std::nullopt,
                                         const std::optional<std::string> &runtime = std::nullopt);

TEST_F(PackageManagerTest, Prune_WhenListingLayersFails_ReturnsError)
{
    repo->onListLocalBy = [](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        LINGLONG_TRACE("listLocalBy");
        return LINGLONG_ERR("cannot list layers");
    };

    auto result = pm->pruneUnused();

    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), HasSubstr("cannot list layers"));
}

TEST_F(PackageManagerTest, Prune_WhenNoLayersExist_OnlyPrunesRepository)
{
    repo->onListLocalBy = [](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{};
    };
    EXPECT_CALL(*repo, mergeModules()).Times(0);
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->pruneUnused();

    ASSERT_TRUE(result) << result.error().message();
}

TEST_F(PackageManagerTest, Prune_ReservesAppBaseAndRuntimeDependencies)
{
    const auto workspace = tempDir->path() / "layer-sources";
    auto appLayer = importLayer(*repo,
                                workspace,
                                "org.example.app",
                                "1.0.0",
                                "app",
                                "binary",
                                "org.deepin.base/23.0.0",
                                "org.deepin.runtime/23.0.0");
    auto baseLayer = importLayer(*repo, workspace, "org.deepin.base", "23.0.0", "base", "binary");
    auto runtimeLayer =
      importLayer(*repo, workspace, "org.deepin.runtime", "23.0.0", "runtime", "runtime");
    ASSERT_TRUE(appLayer);
    ASSERT_TRUE(baseLayer);
    ASSERT_TRUE(runtimeLayer);

    repo->onListLocalBy = [appItem = appLayer->item](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ appItem };
    };
    repo->onClearReferenceLocal =
      [baseRef = baseLayer->reference,
       runtimeRef = runtimeLayer->reference](const package::FuzzyReference &fuzzy,
                                             bool) -> utils::error::Result<package::Reference> {
        if (fuzzy.id == "org.deepin.base") {
            return baseRef;
        }
        if (fuzzy.id == "org.deepin.runtime") {
            return runtimeRef;
        }
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("unexpected reference");
    };
    repo->onGetModuleList = [](const package::Reference &ref) {
        if (ref.id == "org.deepin.runtime") {
            return std::vector<std::string>{ "runtime" };
        }
        return std::vector<std::string>{ "binary" };
    };
    repo->onGetLayerItem =
      [appItem = appLayer->item, baseItem = baseLayer->item, runtimeItem = runtimeLayer->item](
        const package::Reference &ref,
        std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        if (ref.id == "org.example.app") {
            return appItem;
        }
        if (ref.id == "org.deepin.base") {
            return baseItem;
        }
        if (ref.id == "org.deepin.runtime") {
            return runtimeItem;
        }
        LINGLONG_TRACE("getLayerItem");
        return LINGLONG_ERR("unexpected reference");
    };
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->pruneUnused();

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(repo->getLayerDir(appLayer->reference, "binary").has_value())
      << "app layer must survive pruning as a reserved layer";
    EXPECT_TRUE(repo->getLayerDir(baseLayer->reference, "binary").has_value())
      << "base dependency must survive pruning as a reserved layer";
    EXPECT_TRUE(repo->getLayerDir(runtimeLayer->reference, "runtime").has_value())
      << "runtime dependency must survive pruning as a reserved layer";
}

TEST_F(PackageManagerTest, Prune_RemovesUnreferencedLayer)
{
    const auto workspace = tempDir->path() / "layer-sources";
    auto baseLayer = importLayer(*repo, workspace, "org.deepin.base", "23.0.0", "base", "binary");
    ASSERT_TRUE(baseLayer) << "failed to import unreferenced layer fixture";
    const auto deployedLayer = repoDir / "layers" / baseLayer->item.commit;
    ASSERT_TRUE(std::filesystem::exists(deployedLayer))
      << "layer fixture must be deployed before pruning";

    repo->onListLocalBy = [item = baseLayer->item](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ item };
    };
    repo->onGetModuleList = [](const package::Reference &) {
        return std::vector<std::string>{ "binary" };
    };
    repo->onGetLayerItem =
      [item = baseLayer->item](const package::Reference &,
                               std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        return item;
    };
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->pruneUnused();

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(std::filesystem::exists(deployedLayer))
      << "unreferenced layer directory must be deleted";
}

TEST_F(PackageManagerTest, Prune_SkipsNonMainAndInvalidLayers)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64").value();
    auto developItem = makeLayerItem(ref, "app");
    developItem.info.packageInfoV2Module = "develop";

    auto invalidItem = makeLayerItem(ref, "app");
    invalidItem.info.arch.clear();

    repo->onListLocalBy = [developItem, invalidItem](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ developItem, invalidItem };
    };
    EXPECT_CALL(*repo, mergeModules()).Times(0);
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->pruneUnused();

    ASSERT_TRUE(result) << result.error().message();
}

TEST_F(PackageManagerTest, Prune_IgnoresInvalidDependencyReferences)
{
    auto appRef = package::Reference::parse("main:org.example.app/1.0.0/x86_64").value();
    auto appItem = makeLayerItem(appRef, "app");
    appItem.info.base = "not a reference";
    appItem.info.runtime = "also not a reference";

    repo->onListLocalBy = [appItem](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ appItem };
    };
    repo->onGetModuleList = [](const package::Reference &) {
        return std::vector<std::string>{ "binary" };
    };
    repo->onGetLayerItem =
      [appItem](const package::Reference &,
                std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        return appItem;
    };
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    auto result = pm->pruneUnused();

    ASSERT_TRUE(result) << result.error().message();
}

TEST_F(PackageManagerTest, InitRunContext_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->InitRunContext(QStringLiteral("{}"), QStringLiteral("id1"));

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

std::optional<ImportedLayer> importLayer(repo::OSTreeRepo &targetRepo,
                                         const std::filesystem::path &workspace,
                                         const std::string &id,
                                         const std::string &version,
                                         const std::string &kind,
                                         const std::string &module,
                                         const std::optional<std::string> &base,
                                         const std::optional<std::string> &runtime)
{
    const auto layerDir = workspace / (id + "-" + module);
    std::error_code ec;
    std::filesystem::remove_all(layerDir, ec);
    ec.clear();
    std::filesystem::create_directories(layerDir, ec);
    if (ec) {
        ADD_FAILURE() << "failed to create " << layerDir << ": " << ec.message();
        return std::nullopt;
    }

    api::types::v1::PackageInfoV2 info;
    info.arch = std::vector<std::string>{ hostArch() };
    info.base = base.value_or(std::string{});
    info.channel = "stable";
    info.command = std::vector<std::string>{ "echo", id };
    info.id = id;
    info.kind = kind;
    info.packageInfoV2Module = module;
    info.runtime = runtime;
    info.version = version;
    std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();

    auto imported = targetRepo.importLayerDir(package::LayerDir{ layerDir });
    if (!imported) {
        ADD_FAILURE() << "failed to import " << id << "/" << module << ": "
                      << imported.error().message();
        return std::nullopt;
    }

    auto ref = package::Reference::parse("stable:" + id + "/" + version + "/" + hostArch());
    if (!ref) {
        ADD_FAILURE() << ref.error().message();
        return std::nullopt;
    }

    return ImportedLayer{ .reference = *ref, .item = std::move(*imported) };
}

TEST_F(PackageManagerTest, ApplyUnapplyAndSwitchAppUpdateExports)
{
    const auto workspace = tempDir->path() / "layer-sources";
    auto oldLayer = importLayer(*repo, workspace, "org.example.old", "1.0.0", "app", "binary");
    auto newLayer = importLayer(*repo, workspace, "org.example.new", "2.0.0", "app", "binary");
    ASSERT_TRUE(oldLayer);
    ASSERT_TRUE(newLayer);

    const auto entries = repoDir / "entries" / "bin" / "apps";
    const auto oldWrapper = entries / "org.example.old" / "bin" / "org.example.old";
    const auto newWrapper = entries / "org.example.new" / "bin" / "org.example.new";

    ASSERT_TRUE(pm->applyApp(oldLayer->reference))
      << "applyApp should export the old reference entries";
    EXPECT_TRUE(std::filesystem::is_regular_file(oldWrapper));

    ASSERT_TRUE(pm->unapplyApp(oldLayer->reference))
      << "unapplyApp should unexport the old reference entries";
    EXPECT_FALSE(std::filesystem::exists(oldWrapper));

    ASSERT_TRUE(pm->applyApp(newLayer->reference));
    EXPECT_TRUE(std::filesystem::is_regular_file(newWrapper));

    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    ASSERT_TRUE(pm->switchAppVersion(oldLayer->reference, newLayer->reference, false));
    EXPECT_TRUE(std::filesystem::is_regular_file(newWrapper));
    EXPECT_FALSE(std::filesystem::exists(entries / "org.example.old"));
}

TEST_F(PackageManagerTest, UninstallRef_ContinuesWhenModuleRemovalFails)
{
    auto layer = importLayer(*repo,
                             tempDir->path() / "layer-sources",
                             "org.example",
                             "1.0.0",
                             "app",
                             "develop");
    ASSERT_TRUE(layer);
    const auto layerPath = repoDir / "layers" / layer->item.commit;
    ASSERT_TRUE(std::filesystem::exists(layerPath));

    repo->onGetModuleList = [](const package::Reference &) {
        return std::vector<std::string>{ "binary", "develop" };
    };
    repo->onGetLayerItem = [item = layer->item](const package::Reference &, std::string module)
      -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        if (module == "develop") {
            return item;
        }
        LINGLONG_TRACE("getLayerItem");
        return LINGLONG_ERR("missing binary layer");
    };

    auto result = pm->uninstallRef(layer->reference);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_FALSE(std::filesystem::exists(layerPath))
      << "develop module must still be removed after the missing binary module fails";
}

namespace {

class ScopedContainerStates
{
public:
    ScopedContainerStates()
    {
        if (!test::isIsolatedRuntime()) {
            failure = test::isolatedRuntimeRequirement();
            return;
        }

        root = std::filesystem::path{ "/run/linglong" };
        dir = root / std::to_string(::getuid());
        std::error_code ec;
        rootExisted = std::filesystem::exists(root, ec);
        if (ec) {
            failure = "failed to inspect " + root.string() + ": " + ec.message();
            return;
        }

        dirExisted = std::filesystem::exists(dir, ec);
        if (ec) {
            failure = "failed to inspect " + dir.string() + ": " + ec.message();
            return;
        }

        std::filesystem::create_directories(dir, ec);
        if (ec) {
            failure = "failed to create " + dir.string() + ": " + ec.message();
            return;
        }
        dirCreated = !dirExisted;
        rootCreated = !rootExisted;

        const auto probe = dir / (".ll-tests-probe-" + std::to_string(::getpid()));
        int fd = ::open(probe.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd == -1) {
            failure = "container state directory " + dir.string() + " is not writable";
            return;
        }
        ::close(fd);
        std::filesystem::remove(probe, ec);
    }

    ~ScopedContainerStates()
    {
        std::error_code ec;
        for (const auto &file : files) {
            std::filesystem::remove(file, ec);
        }
        if (dirCreated) {
            std::filesystem::remove(dir, ec);
        }
        if (rootCreated) {
            std::filesystem::remove(root, ec);
        }
    }

    bool write(pid_t pid,
               const std::string &app,
               const std::string &base = "",
               const std::optional<std::string> &runtime = std::nullopt,
               const std::optional<std::vector<std::string>> &extensions = std::nullopt)
    {
        nlohmann::json content{ { "app", app }, { "base", base }, { "containerID", "container" } };
        if (runtime) {
            content["runtime"] = *runtime;
        }
        if (extensions) {
            content["extensions"] = *extensions;
        }

        return writeRaw(pid, content.dump());
    }

    bool writeRaw(pid_t pid, const std::string &content)
    {
        if (!failure.empty()) {
            return false;
        }

        auto file = dir / std::to_string(pid);
        int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd == -1) {
            failure = "failed to create " + file.string() + " without overwriting existing state";
            return false;
        }

        const char *data = content.data();
        size_t remaining = content.size();
        while (remaining > 0) {
            auto written = ::write(fd, data, remaining);
            if (written == -1) {
                failure = "failed to write " + file.string();
                break;
            }
            data += written;
            remaining -= static_cast<size_t>(written);
        }
        ::close(fd);
        if (!failure.empty()) {
            std::error_code ec;
            std::filesystem::remove(file, ec);
            return false;
        }

        files.push_back(file);
        return true;
    }

    [[nodiscard]] bool ok() const noexcept { return failure.empty(); }

    [[nodiscard]] const std::string &error() const noexcept { return failure; }

private:
    std::filesystem::path root;
    std::filesystem::path dir;
    std::vector<std::filesystem::path> files;
    bool rootExisted{ false };
    bool dirExisted{ false };
    bool rootCreated{ false };
    bool dirCreated{ false };
    std::string failure;
};

TEST_F(PackageManagerTest, CopyToStagingCopiesRegularFilesAndCleansStaging)
{
    test::ScopedStagingDir staging;
    if (!staging.ok()) {
        GTEST_SKIP() << staging.error();
    }

    const auto source = tempDir->path() / "package.layer";
    std::ofstream{ source } << "layer payload";
    int fd = ::open(source.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeSource = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto staged = pm->copyToStaging(fd);
    ASSERT_TRUE(staged) << staged.error().message();
    EXPECT_EQ(std::filesystem::path(staged->parent_path()).filename(), "staging");
    std::ifstream stagedInput{ *staged };
    std::string payload;
    std::getline(stagedInput, payload);
    EXPECT_EQ(payload, "layer payload");

    auto cleaned = pm->cleanStaging();
    ASSERT_TRUE(cleaned) << cleaned.error().message();
    EXPECT_FALSE(std::filesystem::exists(*staged)) << "staged file must be deleted";
    EXPECT_FALSE(std::filesystem::exists(staging.stagingPath()))
      << "staging directory must be deleted";
}

TEST_F(PackageManagerTest, CopyToStagingRejectsInvalidAndNonRegularDescriptors)
{
    test::ScopedStagingDir staging;
    if (!staging.ok()) {
        GTEST_SKIP() << staging.error();
    }

    auto invalid = pm->copyToStaging(-1);
    EXPECT_FALSE(invalid.has_value());
    EXPECT_THAT(invalid.error().message(), HasSubstr("failed to stat source file"));

    std::array<int, 2> pipeFds{ -1, -1 };
    ASSERT_EQ(::pipe2(pipeFds.data(), O_CLOEXEC), 0);
    auto closePipes = utils::finally::finally([pipeFds] {
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
    });

    auto nonRegular = pm->copyToStaging(pipeFds[0]);
    EXPECT_FALSE(nonRegular.has_value());
    EXPECT_THAT(nonRegular.error().message(), HasSubstr("not a regular file"));
}

} // namespace

} // namespace
