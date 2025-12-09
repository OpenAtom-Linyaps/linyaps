/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/ref_installation.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

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
                installRef,
                (service::Task & task,
                 const package::ReferenceWithRepo &ref,
                 std::vector<std::string> modules),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                switchAppVersion,
                (const package::Reference &oldRef,
                 const package::Reference &newRef,
                 bool removeOld),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                installAppDepends,
                (service::Task & task, const api::types::v1::PackageInfoV2 &info),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                applyApp,
                (const package::Reference &ref),
                (override, noexcept));
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

    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                         .arch = { "x86_64" },
                         .base = "base",
                         .channel = "main",
                         .id = "id",
                         .kind = "app",
                         .packageInfoV2Module = "binary",
                         .runtime = "runtime",
                         .version = "1.0.0",
                       } });
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*pm, installRef(_, _, _)).WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    api::types::v1::RepositoryCacheLayersItem item;
    item.info.kind = "app";
    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(item));

    EXPECT_CALL(*pm, installAppDepends(_, _)).WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));

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

    repo::RemotePackages remote;
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

    repo::RemotePackages remote;
    remote.addPackages(
      api::types::v1::Repo{ .name = "repo" },
      std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                                                    .arch = { "x86_64" },
                                                    .base = "base",
                                                    .channel = "main",
                                                    .id = "id",
                                                    .kind = "app",
                                                    .packageInfoV2Module = "binary",
                                                    .runtime = "runtime",
                                                    .version = "1.0.0",
                                                  },
                                                  api::types::v1::PackageInfoV2{
                                                    .arch = { "x86_64" },
                                                    .base = "base",
                                                    .channel = "main",
                                                    .id = "id",
                                                    .kind = "app",
                                                    .packageInfoV2Module = "develop",
                                                    .runtime = "runtime",
                                                    .version = "1.0.0",
                                                  } });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*pm, installRef(_, _, std::vector<std::string>{ "develop" }))
      .WillOnce(Return(utils::error::Result<void>{}));

    api::types::v1::RepositoryCacheLayersItem item;
    item.info.kind = "app";
    EXPECT_CALL(*repo, getLayerItem(localRef, "binary", _)).WillOnce(Return(item));

    EXPECT_CALL(*pm, installAppDepends(_, _)).WillOnce(Return(utils::error::Result<void>{}));

    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));

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

    repo::RemotePackages remote;
    remote.addPackages(
      api::types::v1::Repo{ .name = "repo" },
      std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                                                    .arch = { "x86_64" },
                                                    .base = "base",
                                                    .channel = "main",
                                                    .id = "id",
                                                    .kind = "app",
                                                    .packageInfoV2Module = "binary",
                                                    .runtime = "runtime",
                                                    .version = "1.0.0",
                                                  },
                                                  api::types::v1::PackageInfoV2{
                                                    .arch = { "x86_64" },
                                                    .base = "base",
                                                    .channel = "main",
                                                    .id = "id",
                                                    .kind = "app",
                                                    .packageInfoV2Module = "develop",
                                                    .runtime = "runtime",
                                                    .version = "1.0.0",
                                                  } });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).WillOnce(Return(std::move(remote)));

    EXPECT_CALL(*pm, installRef(_, _, std::vector<std::string>{ "binary", "develop" }))
      .WillOnce(Return(utils::error::Result<void>{}));

    api::types::v1::RepositoryCacheLayersItem item;
    item.info.kind = "app";
    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(item));

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    EXPECT_CALL(*pm, installAppDepends(_, _)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*pm, applyApp(_)).WillOnce(Return(utils::error::Result<void>{}));

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

    // Local app is newer
    auto localRef = package::Reference::parse("main:id/2.0.0/x86_64").value();
    EXPECT_CALL(*repo, latestLocalReference(_)).WillOnce(Return(localRef));

    // Remote is older
    repo::RemotePackages remote;
    remote.addPackages(api::types::v1::Repo{ .name = "repo" },
                       std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                         .arch = { "x86_64" },
                         .base = "base",
                         .channel = "main",
                         .id = "id",
                         .kind = "app",
                         .packageInfoV2Module = "binary",
                         .runtime = "runtime",
                         .version = "1.0.0",
                       } });

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

    EXPECT_CALL(*pm, installRef(_, _, std::vector<std::string>{ "binary" }))
      .WillOnce(Return(utils::error::Result<void>{}));

    api::types::v1::RepositoryCacheLayersItem item;
    item.info.kind = "app";
    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(item));

    EXPECT_CALL(*pm, installAppDepends(_, _)).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*pm, switchAppVersion(_, _, true)).WillOnce(Return(utils::error::Result<void>{}));

    service::PackageTask task({});
    ASSERT_TRUE(action->prepare());
    auto res = action->doAction(task);
    ASSERT_TRUE(res);
}

} // namespace
