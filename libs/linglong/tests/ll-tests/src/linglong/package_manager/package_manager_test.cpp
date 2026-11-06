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
#define private public
#include "linglong/package_manager/package_manager.h"
#undef private
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

TEST_F(PackageManagerTest, InitRunContext_WhenDaemonNotInitialized_ReturnsError)
{
    auto result = pm->InitRunContext(QStringLiteral("{}"), QStringLiteral("id1"));

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("daemon mode not initialized"));
}

api::types::v1::RepositoryCacheLayersItem makeLayerItemFor(const package::Reference &ref,
                                                           const std::string &kind,
                                                           const std::string &module)
{
    return api::types::v1::RepositoryCacheLayersItem{
        .commit = "commit-" + ref.version.toString() + "-" + module,
        .deleted = std::nullopt,
        .info =
          api::types::v1::PackageInfoV2{
            .arch = std::vector<std::string>{ ref.arch.toString() },
            .channel = ref.channel,
            .id = ref.id,
            .kind = kind,
            .packageInfoV2Module = module,
            .version = ref.version.toString(),
          },
        .repo = "stable",
    };
}

QVariantMap makeUninstallParameters(bool force = false)
{
    api::types::v1::PackageManager1UninstallParameters parameters;
    parameters.options = api::types::v1::CommonOptions{ .force = force,
                                                        .noAutoPrune = false,
                                                        .skipInteraction = false };
    parameters.package.id = "org.example";
    return common::serialize::toQVariantMap(parameters);
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
                                         const std::optional<std::string> &runtime = std::nullopt)
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
    info.arch = std::vector<std::string>{ "x86_64" };
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

    auto ref = package::Reference::parse("stable:" + id + "/" + version + "/x86_64");
    if (!ref) {
        ADD_FAILURE() << ref.error().message();
        return std::nullopt;
    }

    return ImportedLayer{ .reference = *ref, .item = std::move(*imported) };
}

std::optional<std::filesystem::path> packLayer(const std::filesystem::path &workspace,
                                               const std::string &id,
                                               const std::string &version,
                                               const std::string &kind,
                                               const std::string &module,
                                               const std::string &arch = "x86_64")
{
    const auto layerDir = workspace / (id + "-" + version + "-" + module);
    std::error_code ec;
    std::filesystem::create_directories(layerDir / "files", ec);
    if (ec) {
        ADD_FAILURE() << ec.message();
        return std::nullopt;
    }
    std::ofstream{ layerDir / "files" / "payload" } << version;

    api::types::v1::PackageInfoV2 info;
    info.arch = { arch };
    info.channel = "stable";
    info.id = id;
    info.kind = kind;
    info.packageInfoV2Module = module;
    info.version = version;
    std::ofstream{ layerDir / "info.json" } << nlohmann::json(info).dump();

    const auto output = workspace / (id + "-" + version + "-" + module + ".layer");
    std::ofstream{ output };
    package::LayerPackager packager;
    packager.setCompressor("lz4");
    auto packed = packager.pack(package::LayerDir{ layerDir }, QString::fromStdString(output));
    if (!packed) {
        ADD_FAILURE() << packed.error().message();
        return std::nullopt;
    }
    return output;
}

TEST_F(PackageManagerTest, Prune_WhenListingLayersFails_ReturnsError)
{
    repo->onListLocalBy = [](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        LINGLONG_TRACE("listLocalBy");
        return LINGLONG_ERR("cannot list layers");
    };

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), HasSubstr("cannot list layers"));
    EXPECT_TRUE(removed.empty());
}

TEST_F(PackageManagerTest, Prune_WhenNoLayersExist_OnlyPrunesRepository)
{
    repo->onListLocalBy = [](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{};
    };
    EXPECT_CALL(*repo, mergeModules()).Times(0);
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(removed.empty());
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

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(removed.empty());
    EXPECT_TRUE(repo->getLayerDir(appLayer->reference, "binary").has_value())
      << "app layer must survive pruning as a reserved layer";
    EXPECT_TRUE(repo->getLayerDir(baseLayer->reference, "binary").has_value())
      << "base dependency must survive pruning as a reserved layer";
    EXPECT_TRUE(repo->getLayerDir(runtimeLayer->reference, "runtime").has_value())
      << "runtime dependency must survive pruning as a reserved layer";
}

TEST_F(PackageManagerTest, Prune_RemovesUnreferencedLayer)
{
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0/x86_64").value();
    auto baseItem = makeLayerItemFor(baseRef, "base", "binary");

    repo->onListLocalBy = [baseItem](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ baseItem };
    };
    repo->onGetModuleList = [](const package::Reference &) {
        return std::vector<std::string>{ "binary" };
    };
    repo->onGetLayerItem =
      [baseItem](const package::Reference &,
                 std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        return baseItem;
    };
    EXPECT_CALL(*repo, mergeModules()).WillOnce(Return(ok()));
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_TRUE(result) << result.error().message();
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed.front().id, baseRef.id);
}

TEST_F(PackageManagerTest, Prune_SkipsNonMainAndInvalidLayers)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64").value();
    auto developItem = makeLayerItemFor(ref, "app", "develop");

    auto invalidItem = makeLayerItemFor(ref, "app", "binary");
    invalidItem.info.arch.clear();

    repo->onListLocalBy = [developItem, invalidItem](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ developItem, invalidItem };
    };
    EXPECT_CALL(*repo, mergeModules()).Times(0);
    EXPECT_CALL(*repo, prune()).WillOnce(Return(ok()));

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(removed.empty());
}

TEST_F(PackageManagerTest, Prune_IgnoresInvalidDependencyReferences)
{
    auto appRef = package::Reference::parse("main:org.example.app/1.0.0/x86_64").value();
    auto appItem = makeLayerItemFor(appRef, "app", "binary");
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

    std::vector<v1::PackageInfoV2> removed;
    auto result = pm->Prune(removed);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_TRUE(removed.empty());
}

TEST_F(PackageManagerTest, UninstallImpl_RejectsMalformedParameters)
{
    auto result = pm->uninstallImpl(QVariantMap{}, service::CallerContext{});

    EXPECT_NE(replyCode(result), 0);
    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallFailed));
}

TEST_F(PackageManagerTest, UninstallImpl_ReportsPackageNotInstalled)
{
    repo->onListLocalBy = [](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{};
    };

    auto result = pm->uninstallImpl(makeUninstallParameters(), service::CallerContext{});

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallNotFoundFromLocal));
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("not installed"));
}

TEST_F(PackageManagerTest, UninstallImpl_RejectsBaseWithoutForce)
{
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0/x86_64").value();
    auto baseItem = makeLayerItemFor(baseRef, "base", "binary");
    repo->onListLocalBy = [baseItem](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ baseItem };
    };

    auto result = pm->uninstallImpl(makeUninstallParameters(false), service::CallerContext{});

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallBaseOrRuntime));
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("cannot be uninstalled"));
}

TEST_F(PackageManagerTest, UninstallImpl_ReportsMultipleInstalledVersions)
{
    auto ref1 = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    auto ref2 = package::Reference::parse("main:org.example/2.0.0/x86_64").value();
    auto item1 = makeLayerItemFor(ref1, "app", "binary");
    auto item2 = makeLayerItemFor(ref2, "app", "binary");
    repo->onListLocalBy = [item1, item2](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ item1, item2 };
    };

    auto result = pm->uninstallImpl(makeUninstallParameters(), service::CallerContext{});

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallMultipleVersions));
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("main:org.example"));
}

TEST_F(PackageManagerTest, UninstallImpl_IgnoresInvalidPackageInfo)
{
    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    auto item = makeLayerItemFor(ref, "app", "binary");
    item.info.arch.clear();
    repo->onListLocalBy = [item](const linglong::repo::repoCacheQuery &)
      -> utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>> {
        return std::vector<v1::RepositoryCacheLayersItem>{ item };
    };

    auto result = pm->uninstallImpl(makeUninstallParameters(), service::CallerContext{});

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppUninstallNotFoundFromLocal));
}

TEST_F(PackageManagerTest, UpdateImpl_RejectsMalformedParameters)
{
    auto result = pm->updateImpl(QVariantMap{}, service::CallerContext{});

    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppUpgradeFailed));
}

TEST_F(PackageManagerTest, InstallImpl_RejectsMalformedParameters)
{
    auto result = pm->installImpl(QVariantMap{}, service::CallerContext{});

    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::AppInstallFailed));
}

TEST_F(PackageManagerTest, InstallFromFileImpl_RejectsInvalidDescriptor)
{
    auto result = pm->installFromFileImpl(QDBusUnixFileDescriptor{ -1 },
                                          QStringLiteral("layer"),
                                          QVariantMap{},
                                          service::CallerContext{});

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("invalid file descriptor"));
}

TEST_F(PackageManagerTest, InstallFromFileImpl_RejectsMalformedOptions)
{
    auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result = pm->installFromFileImpl(QDBusUnixFileDescriptor{ fd },
                                          QStringLiteral("layer"),
                                          QVariantMap{},
                                          service::CallerContext{});

    EXPECT_EQ(replyCode(result), static_cast<long long>(utils::error::ErrorCode::Failed));
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("failed to parse json"));
}

TEST_F(PackageManagerTest, InstallFromFileImpl_RejectsUnsupportedFileType)
{
    auto fd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });
    auto options =
      common::serialize::toQVariantMap(api::types::v1::CommonOptions{ .force = false,
                                                                      .noAutoPrune = false,
                                                                      .skipInteraction = false });

    auto result = pm->installFromFileImpl(QDBusUnixFileDescriptor{ fd },
                                          QStringLiteral("zip"),
                                          options,
                                          service::CallerContext{});

    EXPECT_EQ(replyCode(result),
              static_cast<long long>(utils::error::ErrorCode::AppInstallUnsupportedFileFormat));
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("unsupported fileType"));
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

TEST_F(PackageManagerTest, RemoveCache_WhenLayerItemMissing_ReturnsError)
{
    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    repo->onGetLayerItem = [](const package::Reference &,
                              std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        LINGLONG_TRACE("getLayerItem");
        return LINGLONG_ERR("missing layer");
    };

    auto result = pm->removeCache(ref);

    EXPECT_FALSE(result);
}

TEST_F(PackageManagerTest, RemoveCache_WhenCacheIsAbsent_Succeeds)
{
    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    auto item = makeLayerItemFor(ref, "app", "binary");
    repo->onGetLayerItem =
      [item](const package::Reference &,
             std::string) -> utils::error::Result<v1::RepositoryCacheLayersItem> {
        return item;
    };

    auto result = pm->removeCache(ref);

    EXPECT_TRUE(result);
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

TEST_F(PackageManagerTest, GetAllRunningContainersReadsLiveProcessesOnly)
{
    ScopedContainerStates states;
    if (!states.ok()) {
        GTEST_SKIP() << states.error();
    }
    if (!states.write(::getpid(), "stable:org.example/1.0.0/x86_64")
        || !states.write(2147483647, "stable:org.example/2.0.0/x86_64")) {
        GTEST_SKIP() << states.error();
    }

    auto running = service::PackageManager::getAllRunningContainers();

    ASSERT_TRUE(running) << running.error().message();
    ASSERT_EQ(running->size(), 1);
    EXPECT_EQ(running->front().app, "stable:org.example/1.0.0/x86_64");
}

TEST_F(PackageManagerTest, GetAllRunningContainersRejectsMalformedStateFiles)
{
    ScopedContainerStates states;
    if (!states.ok()) {
        GTEST_SKIP() << states.error();
    }
    if (!states.writeRaw(::getpid(), "{not-json")) {
        GTEST_SKIP() << states.error();
    }

    auto running = service::PackageManager::getAllRunningContainers();

    EXPECT_FALSE(running.has_value());
}

TEST_F(PackageManagerTest, IsRefBusyDetectsAllTrackedReferences)
{
    const auto appRef = makeRef("1.0.0");
    const auto baseRef = package::Reference::parse("stable:org.example.base/1.0.0/x86_64").value();
    const auto runtimeRef =
      package::Reference::parse("stable:org.example.runtime/1.0.0/x86_64").value();
    const auto extensionRef =
      package::Reference::parse("stable:org.example.extension/1.0.0/x86_64").value();

    ScopedContainerStates states;
    if (!states.ok()) {
        GTEST_SKIP() << states.error();
    }
    if (!states.write(::getpid(),
                      appRef.toString(),
                      baseRef.toString(),
                      runtimeRef.toString(),
                      std::vector<std::string>{ extensionRef.toString() })) {
        GTEST_SKIP() << states.error();
    }

    auto appBusy = pm->isRefBusy(appRef);
    ASSERT_TRUE(appBusy) << appBusy.error().message();
    EXPECT_TRUE(*appBusy);

    for (const auto &ref : { baseRef, runtimeRef, extensionRef }) {
        auto busy = pm->isRefBusy(ref);
        ASSERT_TRUE(busy) << busy.error().message();
        EXPECT_TRUE(*busy) << ref.toString();
    }
}

TEST_F(PackageManagerTest, IsRefBusyReturnsFalseWithoutMatchingContainers)
{
    if (!test::isIsolatedRuntime()) {
        GTEST_SKIP() << test::isolatedRuntimeRequirement();
    }

    auto busy = pm->isRefBusy(makeRef());

    ASSERT_TRUE(busy) << busy.error().message();
    EXPECT_FALSE(*busy);
}

TEST_F(PackageManagerTest, LockRepoCanBeReleasedIdempotently)
{
    test::ScopedRepoLockFile lockFile;
    if (!lockFile.ok()) {
        GTEST_SKIP() << lockFile.error();
    }

    auto locked = pm->lockRepo();
    ASSERT_TRUE(locked) << locked.error().message();

    auto unlocked = pm->unlockRepo();
    ASSERT_TRUE(unlocked) << unlocked.error().message();

    auto unlockedAgain = pm->unlockRepo();
    EXPECT_TRUE(unlockedAgain) << unlockedAgain.error().message();
}

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

TEST_F(PackageManagerTest, DeferredUninstallRemovesMarkedLayers)
{
    test::ScopedRepoLockFile lockFile;
    if (!lockFile.ok()) {
        GTEST_SKIP() << lockFile.error();
    }

    auto layer = importLayer(*repo,
                             tempDir->path() / "deferred-layers",
                             "org.example.deferred",
                             "1.0.0",
                             "app",
                             "binary");
    ASSERT_TRUE(layer);
    repo->onGetLayerItem = [item = layer->item](const package::Reference &, std::string) {
        return utils::error::Result<v1::RepositoryCacheLayersItem>{ item };
    };
    repo->onListLocalBy = [item = layer->item](const repo::repoCacheQuery &) {
        auto deletedItem = item;
        deletedItem.deleted = true;
        return utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>{
            std::vector<v1::RepositoryCacheLayersItem>{ deletedItem }
        };
    };
    const auto layerPath = repoDir / "layers" / layer->item.commit;
    ASSERT_TRUE(std::filesystem::exists(layerPath));
    ASSERT_TRUE(repo->markDeleted(layer->reference, true, "binary"));

    pm->deferredUninstall();

    EXPECT_FALSE(std::filesystem::exists(layerPath));
}

TEST_F(PackageManagerTest, DeferredUninstallKeepsLayersUsedByRunningContainers)
{
    test::ScopedRepoLockFile lockFile;
    if (!lockFile.ok()) {
        GTEST_SKIP() << lockFile.error();
    }

    auto layer = importLayer(*repo,
                             tempDir->path() / "deferred-busy-layers",
                             "org.example.busy",
                             "1.0.0",
                             "app",
                             "binary");
    ASSERT_TRUE(layer);
    repo->onGetLayerItem = [item = layer->item](const package::Reference &, std::string) {
        return utils::error::Result<v1::RepositoryCacheLayersItem>{ item };
    };
    repo->onListLocalBy = [item = layer->item](const repo::repoCacheQuery &) {
        auto deletedItem = item;
        deletedItem.deleted = true;
        return utils::error::Result<std::vector<v1::RepositoryCacheLayersItem>>{
            std::vector<v1::RepositoryCacheLayersItem>{ deletedItem }
        };
    };
    const auto layerPath = repoDir / "layers" / layer->item.commit;
    ASSERT_TRUE(std::filesystem::exists(layerPath));
    ASSERT_TRUE(repo->markDeleted(layer->reference, true, "binary"));

    {
        ScopedContainerStates states;
        if (!states.ok()) {
            GTEST_SKIP() << states.error();
        }
        if (!states.write(::getpid(), layer->reference.toString())) {
            GTEST_SKIP() << states.error();
        }

        pm->deferredUninstall();

        EXPECT_TRUE(std::filesystem::exists(layerPath));
    }

    pm->deferredUninstall();

    EXPECT_FALSE(std::filesystem::exists(layerPath));
}

TEST_F(PackageManagerTest, InstallFromLayerRejectsAlreadyInstalledAndDowngrade)
{
    auto local = importLayer(*repo,
                             tempDir->path() / "installed-layers",
                             "org.example.install",
                             "2.0.0",
                             "app",
                             "binary");
    ASSERT_TRUE(local);
    repo->onClearReferenceLocal = [this](const package::FuzzyReference &fuzzy,
                                         bool semanticMatching) {
        return repo->OSTreeRepo::clearReferenceLocal(fuzzy, semanticMatching);
    };
    repo->onGetLayerItem = [item = local->item](const package::Reference &, std::string) {
        return utils::error::Result<v1::RepositoryCacheLayersItem>{ item };
    };

    const auto workspace = tempDir->path() / "packed-layers";
    auto sameVersion = packLayer(workspace, "org.example.install", "2.0.0", "app", "binary");
    ASSERT_TRUE(sameVersion);
    int sameFd = ::open(sameVersion->c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(sameFd, -1);
    auto closeSame = utils::finally::finally([sameFd] {
        ::close(sameFd);
    });

    auto sameResult =
      pm->installFromFileImpl(QDBusUnixFileDescriptor{ sameFd },
                              "layer",
                              common::serialize::toQVariantMap(api::types::v1::CommonOptions{}),
                              service::CallerContext{});
    EXPECT_NE(replyCode(sameResult), 0);
    EXPECT_THAT(replyMessage(sameResult).toStdString(), HasSubstr("already installed"));

    auto olderVersion = packLayer(workspace, "org.example.install", "1.0.0", "app", "binary");
    ASSERT_TRUE(olderVersion);
    int olderFd = ::open(olderVersion->c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(olderFd, -1);
    auto closeOlder = utils::finally::finally([olderFd] {
        ::close(olderFd);
    });

    auto olderResult =
      pm->installFromFileImpl(QDBusUnixFileDescriptor{ olderFd },
                              "layer",
                              common::serialize::toQVariantMap(api::types::v1::CommonOptions{}),
                              service::CallerContext{});
    EXPECT_NE(replyCode(olderResult), 0);
    EXPECT_THAT(replyMessage(olderResult).toStdString(), HasSubstr("latest version"));
}

TEST_F(PackageManagerTest, InstallFromLayerRejectsArchitectureMismatch)
{
    const auto workspace = tempDir->path() / "packed-layers";
    const auto foreignArch =
      package::Architecture::currentCPUArchitecture().toString() == "x86_64" ? "arm64" : "x86_64";
    auto layer = packLayer(workspace, "org.example.foreign", "1.0.0", "app", "binary", foreignArch);
    ASSERT_TRUE(layer);
    int fd = ::open(layer->c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result =
      pm->installFromFileImpl(QDBusUnixFileDescriptor{ fd },
                              "layer",
                              common::serialize::toQVariantMap(api::types::v1::CommonOptions{}),
                              service::CallerContext{});

    EXPECT_NE(replyCode(result), 0);
    EXPECT_THAT(replyMessage(result).toStdString(), HasSubstr("not match host architecture"));
}

TEST_F(PackageManagerTest, InstallFromLayerRejectsMalformedArchive)
{
    const auto malformed = tempDir->path() / "malformed.layer";
    std::ofstream{ malformed } << "not an erofs image";
    int fd = ::open(malformed.c_str(), O_RDONLY | O_CLOEXEC);
    ASSERT_NE(fd, -1);
    auto closeFd = utils::finally::finally([fd] {
        ::close(fd);
    });

    auto result =
      pm->installFromFileImpl(QDBusUnixFileDescriptor{ fd },
                              "layer",
                              common::serialize::toQVariantMap(api::types::v1::CommonOptions{}),
                              service::CallerContext{});

    EXPECT_NE(replyCode(result), 0);
}

} // namespace

} // namespace
