/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/common/serialize/json.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/utils/error/error.h"

#include <QString>
#include <QVariant>
#include <QVariantMap>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
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

package::FuzzyReference makeFuzzyRef()
{
    return package::FuzzyReference::parse("main:org.example/1.0.0/x86_64").value();
}

package::ReferenceWithRepo makeRefWithRepo()
{
    return package::ReferenceWithRepo{ .repo = api::types::v1::Repo{ .name = "stable" },
                                       .reference = makeRef() };
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
        return LINGLONG_ERR("remote down");
    };

    auto result = pm->installDependsRef(taskPlaceholder, "org.example");

    EXPECT_FALSE(result.has_value());
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
    repo->onMatchRemoteByPriority = [](const auto &, const auto &) {
        LINGLONG_TRACE("matchRemoteByPriority");
        return LINGLONG_ERR("remote down");
    };

    auto result = pm->needToUpgrade(makeFuzzyRef(), local, false);

    EXPECT_FALSE(result.has_value());
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

    auto installed = makeRef();
    repo->onClearReferenceLocal = [installed](const auto &, bool) {
        return utils::error::Result<package::Reference>{ installed };
    };

    auto result = pm->installAppDepends(taskPlaceholder, app);

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, InstallAppDepends_WithRuntime_InstallsBaseAndRuntime)
{
    service::Task taskPlaceholder;
    v1::PackageInfoV2 app = makeAppInfo();
    app.base = "org.base";
    app.runtime = "org.runtime";

    auto installed = makeRef();
    repo->onClearReferenceLocal = [installed](const auto &, bool) {
        return utils::error::Result<package::Reference>{ installed };
    };

    auto result = pm->installAppDepends(taskPlaceholder, app);

    EXPECT_TRUE(result.has_value());
}

TEST_F(PackageManagerTest, Uninstall_WhenGetLayerItemFails_ReturnsError)
{
    service::PackageTask taskPlaceholder([](service::Task &) { }, nullptr);

    auto result = pm->Uninstall(taskPlaceholder, makeRef(), "binary", false);

    EXPECT_FALSE(result.has_value());
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

} // namespace
