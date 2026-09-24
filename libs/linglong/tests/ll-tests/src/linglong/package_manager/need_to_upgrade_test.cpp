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
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/repo/remote_packages.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::Return;

api::types::v1::PackageInfoV2 makeRemotePackage(const std::string &module)
{
    return api::types::v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .base = "",
        .channel = "main",
        .id = "org.test.app",
        .kind = "app",
        .packageInfoV2Module = module,
        .name = "test app",
        .version = "1.0.0",
    };
}

class NeedToUpgradeRepo : public repo::OSTreeRepo
{
public:
    explicit NeedToUpgradeRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReferenceLocal,
                (const package::FuzzyReference &, bool),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RemotePackages>,
                matchRemoteByPriority,
                (const package::FuzzyReference &, const std::optional<api::types::v1::Repo> &),
                (override, const, noexcept));

    MOCK_METHOD(std::vector<std::string>,
                getModuleList,
                (const package::Reference &),
                (override, const, noexcept));
};

// Uses the real PackageManager::needToUpgrade implementation.
class NeedToUpgradePackageManager : public service::PackageManager
{
public:
    NeedToUpgradePackageManager(std::unique_ptr<repo::OSTreeRepo> repo,
                                std::unique_ptr<runtime::ContainerBuilder> builder,
                                QObject *parent)
        : service::PackageManager(std::move(repo), std::move(builder), parent)
    {
    }
};

class NeedToUpgradeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        auto repoOwner = std::make_unique<NeedToUpgradeRepo>(tempDir->path());
        repo = repoOwner.get();
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        auto containerBuilderOwner = std::make_unique<runtime::ContainerBuilder>(*cli);
        pm = std::make_unique<NeedToUpgradePackageManager>(std::move(repoOwner),
                                                           std::move(containerBuilderOwner),
                                                           nullptr);
    }

    void TearDown() override
    {
        pm.reset();
        cli.reset();
        tempDir.reset();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    NeedToUpgradeRepo *repo{ nullptr };
    std::unique_ptr<NeedToUpgradePackageManager> pm;
};

// Regression: installing a missing package whose remote entry has no binary
// module used to dereference the empty optional `local` while formatting the
// error message, which is undefined behaviour.
TEST_F(NeedToUpgradeTest, MissingLocalWithoutBinaryModuleReturnsError)
{
    auto fuzzy =
      package::FuzzyReference::create(std::nullopt, "org.test.app", std::nullopt, std::nullopt);
    ASSERT_TRUE(fuzzy.has_value());

    EXPECT_CALL(*repo, clearReferenceLocal(_, _))
      .WillOnce(
        [](const package::FuzzyReference &, bool) -> utils::error::Result<package::Reference> {
            LINGLONG_TRACE("clearReferenceLocal");
            return LINGLONG_ERR("app not found locally",
                                utils::error::ErrorCode::AppNotFoundFromLocal);
        });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce([](const package::FuzzyReference &, const std::optional<api::types::v1::Repo> &)
                  -> utils::error::Result<repo::RemotePackages> {
          repo::RemotePackages packages;
          packages.addPackages(api::types::v1::Repo{ .name = "stable",
                                                     .priority = 0,
                                                     .url = "https://example.com/repo" },
                               { makeRemotePackage("custom") });
          return packages;
      });

    std::optional<package::Reference> local;
    auto result = pm->needToUpgrade(*fuzzy, local, /*installIfMissing=*/true);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), static_cast<int>(utils::error::ErrorCode::AppUpgradeFailed));
    EXPECT_THAT(result.error().message(), testing::HasSubstr("no modules found for"));
    EXPECT_FALSE(local.has_value());
}

TEST_F(NeedToUpgradeTest, MissingLocalWithBinaryModuleInstallsBinary)
{
    auto fuzzy =
      package::FuzzyReference::create(std::nullopt, "org.test.app", std::nullopt, std::nullopt);
    ASSERT_TRUE(fuzzy.has_value());

    EXPECT_CALL(*repo, clearReferenceLocal(_, _))
      .WillOnce(
        [](const package::FuzzyReference &, bool) -> utils::error::Result<package::Reference> {
            LINGLONG_TRACE("clearReferenceLocal");
            return LINGLONG_ERR("app not found locally",
                                utils::error::ErrorCode::AppNotFoundFromLocal);
        });

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce([](const package::FuzzyReference &, const std::optional<api::types::v1::Repo> &)
                  -> utils::error::Result<repo::RemotePackages> {
          repo::RemotePackages packages;
          packages.addPackages(api::types::v1::Repo{ .name = "stable",
                                                     .priority = 0,
                                                     .url = "https://example.com/repo" },
                               { makeRemotePackage("binary") });
          return packages;
      });

    std::optional<package::Reference> local;
    auto result = pm->needToUpgrade(*fuzzy, local, /*installIfMissing=*/true);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
    EXPECT_EQ(result->value().first.reference.id, "org.test.app");
    EXPECT_EQ(result->value().second, std::vector<std::string>{ "binary" });
    EXPECT_FALSE(local.has_value());
}

TEST_F(NeedToUpgradeTest, LocalUpgradeWithNoMatchingModulesReturnsError)
{
    auto fuzzy =
      package::FuzzyReference::create(std::nullopt, "org.test.app", std::nullopt, std::nullopt);
    ASSERT_TRUE(fuzzy.has_value());

    auto localRef = package::Reference::create("main",
                                               "org.test.app",
                                               package::Version::parse("0.9.0").value(),
                                               package::Architecture::parse("x86_64").value());
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce([](const package::FuzzyReference &, const std::optional<api::types::v1::Repo> &)
                  -> utils::error::Result<repo::RemotePackages> {
          repo::RemotePackages packages;
          packages.addPackages(api::types::v1::Repo{ .name = "stable",
                                                     .priority = 0,
                                                     .url = "https://example.com/repo" },
                               { makeRemotePackage("custom") });
          return packages;
      });

    EXPECT_CALL(*repo, getModuleList(_)).WillOnce(Return(std::vector<std::string>{ "binary" }));

    std::optional<package::Reference> local{ *localRef };
    auto result = pm->needToUpgrade(*fuzzy, local, /*installIfMissing=*/false);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(), static_cast<int>(utils::error::ErrorCode::AppUpgradeFailed));
    EXPECT_THAT(result.error().message(), testing::HasSubstr("no modules found for"));
    EXPECT_TRUE(local.has_value());
}

} // namespace
