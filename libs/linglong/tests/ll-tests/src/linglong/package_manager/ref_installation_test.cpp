/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/ref_installation.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <nlohmann/json.hpp>

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Return;

class MockPackageManager : public service::PackageManager
{
public:
    MockPackageManager(repo::OSTreeRepo &repo, runtime::ContainerBuilder &builder, QObject *parent)
        : service::PackageManager(repo, builder, parent)
    {
    }

    MOCK_METHOD(utils::error::Result<void>,
                installRefModule,
                (service::Task & task,
                 const package::ReferenceWithRepo &ref,
                 const std::string &module),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                switchAppVersion,
                (const package::Reference &oldRef,
                 const package::Reference &newRef,
                 bool removeOld),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                applyApp,
                (const package::Reference &ref),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<std::optional<package::ReferenceWithRepo>>,
                needToInstall,
                (const std::string &refStr, std::optional<std::string> channel),
                (override));
};

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<package::Reference>,
                latestLocalReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref,
                 std::string module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RemotePackages>,
                matchRemoteByPriority,
                (const package::FuzzyReference &fuzzyRef,
                 const std::optional<api::types::v1::Repo> &repo),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>>,
                listLocalBy,
                (const repo::repoCacheQuery &query),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RefMetaData>,
                fetchRefMetaData,
                (const package::ReferenceWithRepo &ref, const std::string &module, bool fetchInfo),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RefStatistics>,
                getRefStatistics,
                (const repo::RefMetaData &meta),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>, mergeModules, (), (override, const, noexcept));
};

class RefInstallationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        repo = std::make_unique<MockRepo>(tempDir->path());
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm = std::make_unique<MockPackageManager>(*repo, *containerBuilder, nullptr);
    }

    void TearDown() override
    {
        pm.reset();
        containerBuilder.reset();
        cli.reset();
        repo.reset();
        tempDir.reset();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<MockRepo> repo;
    std::unique_ptr<MockPackageManager> pm;
};

TEST_F(RefInstallationTest, InstallApp)
{
    LINGLONG_TRACE("InstallApp");

    auto fuzzy = package::FuzzyReference::parse("main:id/1.0.0/x86_64").value();
    std::vector<std::string> modules{ "binary" };
    api::types::v1::CommonOptions opts{ .force = false, .skipInteraction = true };
    auto action =
      service::RefInstallationAction::create(fuzzy, modules, *pm, *repo, opts, std::nullopt);

    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(LINGLONG_ERR("")));

    api::types::v1::PackageInfoV2 info{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ info });
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "binary", true))
      .WillOnce(Return(repo::RefMetaData{ "rev123", nlohmann::json(info).dump() }));

    EXPECT_CALL(*repo, getRefStatistics(_)).WillOnce([](const repo::RefMetaData &) {
        return utils::error::Result<repo::RefStatistics>{
            repo::RefStatistics{ .archived = 1000, .needed_archived = 500 }
        };
    });

    EXPECT_CALL(*pm, installRefModule(_, _, "binary"))
      .WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, needToInstall("base", _)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*pm, needToInstall("runtime", _)).WillOnce(Return(std::nullopt));

    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*repo, mergeModules()).WillOnce([]() {
        return utils::error::Result<void>{};
    });

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    ASSERT_TRUE(action->doAction(task));
}

TEST_F(RefInstallationTest, InstallNoAppFound)
{
    LINGLONG_TRACE("InstallNoAppFound");

    auto fuzzy = package::FuzzyReference::parse("main:id/1.0.0/x86_64").value();
    std::vector<std::string> modules{ "binary" };
    api::types::v1::CommonOptions opts{ .force = false, .skipInteraction = true };
    auto action =
      service::RefInstallationAction::create(fuzzy, modules, *pm, *repo, opts, std::nullopt);

    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(LINGLONG_ERR("")));

    repo::RemotePackages remote; // Empty remote packages
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    auto res = action->doAction(task);
    ASSERT_FALSE(res);
    EXPECT_EQ(res.error().code(),
              static_cast<int>(utils::error::ErrorCode::AppInstallNotFoundFromRemote));
}

TEST_F(RefInstallationTest, InstallExtraOnly)
{
    LINGLONG_TRACE("InstallExtraOnly");

    auto fuzzy = package::FuzzyReference::parse("main:id/1.0.0/x86_64").value();
    std::vector<std::string> modules{ "develop" };
    api::types::v1::CommonOptions opts{ .force = false, .skipInteraction = true };
    auto action =
      service::RefInstallationAction::create(fuzzy, modules, *pm, *repo, opts, std::nullopt);

    auto localRef = package::Reference::parse("main:id/1.0.0/x86_64").value();
    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(localRef));

    EXPECT_CALL(*repo, getLayerItem(localRef, "develop", _))
      .WillOnce(Return(LINGLONG_ERR("not found")));

    api::types::v1::PackageInfoV2 infoBinary{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    api::types::v1::PackageInfoV2 infoDevelop{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "develop",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ infoBinary, infoDevelop });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "develop", true))
      .WillOnce(Return(repo::RefMetaData{ "rev124", nlohmann::json(infoDevelop).dump() }));

    EXPECT_CALL(*repo, getRefStatistics(_)).WillOnce([](const repo::RefMetaData &) {
        return utils::error::Result<repo::RefStatistics>{
            repo::RefStatistics{ .archived = 1000, .needed_archived = 500 }
        };
    });

    EXPECT_CALL(*pm, installRefModule(_, _, "develop"))
      .WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, needToInstall("base", _)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*pm, needToInstall("runtime", _)).WillOnce(Return(std::nullopt));

    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*repo, mergeModules()).WillOnce([]() {
        return utils::error::Result<void>{};
    });

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    ASSERT_TRUE(action->doAction(task));
}

TEST_F(RefInstallationTest, InstallMultipleModules)
{
    LINGLONG_TRACE("InstallMultipleModules");

    auto fuzzy = package::FuzzyReference::parse("main:id/1.0.0/x86_64").value();
    std::vector<std::string> modules{ "binary", "develop" };
    api::types::v1::CommonOptions opts{ .force = false, .skipInteraction = true };
    auto action =
      service::RefInstallationAction::create(fuzzy, modules, *pm, *repo, opts, std::nullopt);

    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(LINGLONG_ERR("")));

    api::types::v1::PackageInfoV2 infoBinary{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    api::types::v1::PackageInfoV2 infoDevelop{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "develop",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ infoBinary, infoDevelop });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "binary", true))
      .WillOnce(Return(repo::RefMetaData{ "rev123", nlohmann::json(infoBinary).dump() }));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "develop", false))
      .WillOnce(Return(repo::RefMetaData{ "rev124", nlohmann::json(infoDevelop).dump() }));

    EXPECT_CALL(*repo, getRefStatistics(_)).WillRepeatedly([](const repo::RefMetaData &) {
        return utils::error::Result<repo::RefStatistics>{
            repo::RefStatistics{ .archived = 1000, .needed_archived = 500 }
        };
    });

    EXPECT_CALL(*pm, installRefModule(_, _, "binary"))
      .WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*pm, installRefModule(_, _, "develop"))
      .WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, needToInstall("base", _)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*pm, needToInstall("runtime", _)).WillOnce(Return(std::nullopt));

    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*repo, mergeModules()).WillOnce([]() {
        return utils::error::Result<void>{};
    });

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    ASSERT_TRUE(action->doAction(task));
}

TEST_F(RefInstallationTest, InstallDowngrade)
{
    LINGLONG_TRACE("InstallDowngrade");

    auto fuzzy = package::FuzzyReference::parse("main:id/1.0.0/x86_64").value();
    std::vector<std::string> modules{ "binary" };
    api::types::v1::CommonOptions opts{ .force = true, .skipInteraction = true };
    auto action =
      service::RefInstallationAction::create(fuzzy, modules, *pm, *repo, opts, std::nullopt);

    auto localRef = package::Reference::parse("main:id/2.0.0/x86_64").value();
    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(localRef));

    api::types::v1::PackageInfoV2 info{
        .arch = { "x86_64" },
        .base = "base",
        .channel = "main",
        .id = "id",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .runtime = "runtime",
        .version = "1.0.0",
    };

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ info });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = {
            .arch = { "x86_64" },
            .base = "base",
            .channel = "main",
            .id = "id",
            .kind = "app",
            .packageInfoV2Module = "binary",
            .runtime = "runtime",
            .version = "2.0.0",
          },
        } }));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "binary", true))
      .WillOnce(Return(repo::RefMetaData{ "rev123", nlohmann::json(info).dump() }));

    EXPECT_CALL(*repo, getRefStatistics(_)).WillOnce([](const repo::RefMetaData &) {
        return utils::error::Result<repo::RefStatistics>{
            repo::RefStatistics{ .archived = 1000, .needed_archived = 500 }
        };
    });

    EXPECT_CALL(*pm, installRefModule(_, _, "binary"))
      .WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, needToInstall("base", _)).WillOnce(Return(std::nullopt));
    EXPECT_CALL(*pm, needToInstall("runtime", _)).WillOnce(Return(std::nullopt));

    EXPECT_CALL(*pm, switchAppVersion(_, _, true)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*repo, mergeModules()).WillOnce([]() {
        return utils::error::Result<void>{};
    });

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    auto res = action->doAction(task);
    ASSERT_TRUE(res);
}

} // namespace
