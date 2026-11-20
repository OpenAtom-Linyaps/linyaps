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

#include <atomic>
#include <thread>
#include <vector>

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
        setTaskName("mock_action");
    }

    using Action::getTaskMessage;
    using Action::setTaskMessage;
    using Action::setTaskName;

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

TEST_F(ActionTest, TaskTextSnapshotsSurviveLaterUpdates)
{
    EXPECT_EQ(mockAction->getTaskName(), "mock_action");
    EXPECT_TRUE(mockAction->getTaskMessage().empty());
    mockAction->setTaskName("Install application");
    mockAction->setTaskMessage(std::string(4096, 'a'));
    const auto name = mockAction->getTaskName();
    const auto message = mockAction->getTaskMessage();

    mockAction->setTaskName("Update application");
    mockAction->setTaskMessage(std::string(8192, 'b'));
    EXPECT_EQ(name, "Install application");
    EXPECT_EQ(message, std::string(4096, 'a'));
    EXPECT_EQ(mockAction->getTaskName(), "Update application");
    EXPECT_EQ(mockAction->getTaskMessage(), std::string(8192, 'b'));
}

TEST_F(ActionTest, TaskTextSupportsConcurrentReadersAndWriters)
{
    const std::string first(4096, 'a');
    const std::string second(8192, 'b');
    mockAction->setTaskName(first);
    mockAction->setTaskMessage(first);
    std::atomic<bool> start{ false };
    std::atomic<bool> valid{ true };
    std::vector<std::thread> threads;

    threads.emplace_back([&] {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 3000; ++i) {
            const auto &text = i % 2 == 0 ? second : first;
            mockAction->setTaskName(text);
            mockAction->setTaskMessage(text);
        }
    });
    for (int reader = 0; reader < 3; ++reader) {
        threads.emplace_back([&] {
            while (!start.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < 3000; ++i) {
                const auto name = mockAction->getTaskName();
                const auto message = mockAction->getTaskMessage();
                if ((name != first && name != second) || (message != first && message != second)) {
                    valid.store(false);
                }
            }
        });
    }
    start.store(true);
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_TRUE(valid.load());
}

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

TEST(UninstallCandidateTest, SelectsStandaloneRequestedModule)
{
    const std::vector<api::types::v1::RepositoryCacheLayersItem> layers{
        { .info = { .arch = { "x86_64" },
                    .channel = "main",
                    .id = "org.example.App",
                    .kind = "app",
                    .packageInfoV2Module = "develop",
                    .version = "1.0.0" } },
    };

    const auto requested = service::selectUninstallCandidates(layers, "develop");
    ASSERT_EQ(requested.size(), 1);
    EXPECT_EQ(requested.front().ref.toString(), "main:org.example.App/1.0.0/x86_64");
    EXPECT_EQ(requested.front().module, "develop");
    EXPECT_TRUE(service::selectUninstallCandidates(layers, std::nullopt).empty());
}

TEST(UninstallCandidateTest, SelectsOnlyTheRequestedVersionAndModule)
{
    const std::vector<api::types::v1::RepositoryCacheLayersItem> layers{
        { .info = { .arch = { "x86_64" },
                    .channel = "main",
                    .id = "org.example.App",
                    .kind = "app",
                    .packageInfoV2Module = "binary",
                    .version = "1.0.0" } },
        { .info = { .arch = { "x86_64" },
                    .channel = "main",
                    .id = "org.example.App",
                    .kind = "app",
                    .packageInfoV2Module = "develop",
                    .version = "2.0.0" } },
    };

    const auto requested = service::selectUninstallCandidates(layers, "develop");
    ASSERT_EQ(requested.size(), 1);
    EXPECT_EQ(requested.front().ref.toString(), "main:org.example.App/2.0.0/x86_64");
    EXPECT_EQ(requested.front().module, "develop");
}

TEST(UninstallCandidateTest, CountsMainModulesOfOneReferenceOnlyOnce)
{
    const std::vector<api::types::v1::RepositoryCacheLayersItem> layers{
        { .info = { .arch = { "x86_64" },
                    .channel = "main",
                    .id = "org.example.App",
                    .kind = "app",
                    .packageInfoV2Module = "runtime",
                    .version = "1.0.0" } },
        { .info = { .arch = { "x86_64" },
                    .channel = "main",
                    .id = "org.example.App",
                    .kind = "app",
                    .packageInfoV2Module = "binary",
                    .version = "1.0.0" } },
    };

    const auto matches = service::selectUninstallCandidates(layers, std::nullopt);
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches.front().module, "binary");
}

} // namespace
