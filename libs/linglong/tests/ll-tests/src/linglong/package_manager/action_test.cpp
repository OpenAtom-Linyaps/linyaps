/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/package_manager/action.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

namespace linglong::runtime {
class ContainerBuilder;
}

namespace linglong::service {
class PackageTask;
}

using namespace linglong;
using ::testing::_;
using ::testing::Return;

namespace {

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<void>,
                markDeleted,
                (const package::Reference &, bool, const std::string &),
                (override, noexcept));
    MOCK_METHOD(std::vector<std::string>,
                getModuleList,
                (const package::Reference &),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>>,
                listLocalBy,
                (const repo::repoCacheQuery &query),
                (override, const, noexcept));
};

class MockAction : public service::Action
{
public:
    MockAction(service::PackageManager &pm,
               repo::OSTreeRepo &repo,
               api::types::v1::CommonOptions opts)
        : service::Action(pm, repo, opts)
    {
    }

    utils::error::Result<service::ActionOperation>
    testActionOperation(const api::types::v1::PackageInfoV2 &target, bool extraModuleOnly)
    {
        return getActionOperation(target, extraModuleOnly);
    }

    utils::error::Result<void> prepare() override { return utils::error::Result<void>(); }

    utils::error::Result<void> doAction([[maybe_unused]] service::PackageTask &task) override
    {
        return utils::error::Result<void>();
    }

    std::string getTaskName() const override { return "mock_action"; }
};

class ActionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        auto repoOwner = std::make_unique<MockRepo>(tempDir->path());
        repo = repoOwner.get();
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        auto containerBuilderOwner = std::make_unique<runtime::ContainerBuilder>(*cli);
        containerBuilder = containerBuilderOwner.get();
        pm = std::make_unique<service::PackageManager>(std::move(repoOwner),
                                                       std::move(containerBuilderOwner),
                                                       nullptr);
        mockAction = std::make_unique<MockAction>(*pm, *repo, api::types::v1::CommonOptions());
    }

    void TearDown() override
    {
        mockAction.reset();
        pm.reset();
        cli.reset();
        tempDir.reset();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    runtime::ContainerBuilder *containerBuilder{ nullptr };
    MockRepo *repo{ nullptr };
    std::unique_ptr<service::PackageManager> pm;
    std::unique_ptr<MockAction> mockAction;
};

TEST_F(ActionTest, InstallNewApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, OverwriteApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = packageInfo,
        } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Overwrite);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, UpgradeApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "2.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "app",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "1.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Upgrade);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/2.0.0/x86_64");
}

TEST_F(ActionTest, DowngradeApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "app",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "2.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Downgrade);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/2.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, InstallLowVersionRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "runtime",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "2.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, InstallHighVersionRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "2.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "runtime",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "1.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/2.0.0/x86_64");
}

TEST_F(ActionTest, InstallSameRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_)).WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
            .info = {
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "id1",
                .kind = "runtime",
                .packageInfoV2Module = "binary",
                .version = "2.0.0",
            }
        },
        api::types::v1::RepositoryCacheLayersItem{
            .info = {
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "id1",
                .kind = "runtime",
                .packageInfoV2Module = "binary",
                .version = "1.0.0",
            }
        }
    }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Overwrite);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

} // namespace

namespace {

class BusyPackageManager : public service::PackageManager
{
public:
    using service::PackageManager::PackageManager;
    MOCK_METHOD(utils::error::Result<bool>,
                isRefBusy,
                (const package::Reference &),
                (override, noexcept));
};

class DeferredUninstallTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        auto ownedRepo = std::make_unique<MockRepo>(temp.path());
        repo = ownedRepo.get();
        auto createdRuntime = ocppi::cli::crun::Crun::New(temp.path());
        ASSERT_TRUE(createdRuntime.has_value());
        cli = std::move(*createdRuntime);
        auto builder = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm =
          std::make_unique<BusyPackageManager>(std::move(ownedRepo), std::move(builder), nullptr);
    }

    TempDir temp;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    MockRepo *repo = nullptr;
    std::unique_ptr<BusyPackageManager> pm;
};

TEST_F(DeferredUninstallTest, FailedLaterMarkRollsBackEarlierMarks)
{
    auto ref = package::Reference::parse("main:com.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());
    EXPECT_CALL(*pm, isRefBusy(*ref)).WillOnce(Return(true));
    EXPECT_CALL(*repo, getModuleList(*ref))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop", "debug" }));
    testing::InSequence sequence;
    EXPECT_CALL(*repo, markDeleted(*ref, true, "binary"))
      .WillOnce([](const auto &, bool, const auto &) -> utils::error::Result<void> {
          return LINGLONG_OK;
      });
    EXPECT_CALL(*repo, markDeleted(*ref, true, "develop"))
      .WillOnce([](const auto &, bool, const auto &) -> utils::error::Result<void> {
          LINGLONG_TRACE("inject deferred deletion failure");
          return LINGLONG_ERR("injected persistence failure");
      });
    EXPECT_CALL(*repo, markDeleted(*ref, false, "binary"))
      .WillOnce([](const auto &, bool, const auto &) -> utils::error::Result<void> {
          return LINGLONG_OK;
      });
    EXPECT_CALL(*repo, markDeleted(*ref, true, "debug")).Times(0);
    auto result = pm->tryUninstallRef(*ref);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("injected persistence failure"), std::string::npos);
}

TEST_F(DeferredUninstallTest, SuccessfulMarksRemainDeferred)
{
    auto ref = package::Reference::parse("main:com.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());
    EXPECT_CALL(*pm, isRefBusy(*ref)).WillOnce(Return(true));
    EXPECT_CALL(*repo, getModuleList(*ref))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));
    EXPECT_CALL(*repo, markDeleted(*ref, true, "binary"))
      .WillOnce([](const auto &, bool, const auto &) -> utils::error::Result<void> {
          return LINGLONG_OK;
      });
    EXPECT_CALL(*repo, markDeleted(*ref, true, "develop"))
      .WillOnce([](const auto &, bool, const auto &) -> utils::error::Result<void> {
          return LINGLONG_OK;
      });
    EXPECT_CALL(*repo, markDeleted(*ref, false, _)).Times(0);
    auto result = pm->tryUninstallRef(*ref);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(*result);
}

} // namespace
