/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

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
        repo = std::make_unique<MockRepo>(tempDir->path());
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm = std::make_unique<service::PackageManager>(*repo, *containerBuilder, nullptr);
        mockAction = std::make_unique<MockAction>(*pm, *repo, api::types::v1::CommonOptions());
    }

    void TearDown() override
    {
        mockAction.reset();
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
