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
#include "linglong/package_manager/package_update.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::IsSubsetOf;
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

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>,
                listLocalApps,
                (),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReference,
                (const package::FuzzyReference &fuzzy,
                 const repo::clearReferenceOption &opts,
                 const std::string &module,
                 const std::optional<std::string> &repo),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref,
                 std::string module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));

    MOCK_METHOD(std::vector<std::string>,
                getModuleList,
                (const package::Reference &ref),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<std::vector<std::string>>,
                getRemoteModuleList,
                (const package::Reference &ref, const api::types::v1::Repo &repo),
                (override, const, noexcept));
};

class PackageUpdateActionTest : public ::testing::Test
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

TEST_F(PackageUpdateActionTest, Update)
{
    auto action =
      service::PackageUpdateAction::create(std::vector<api::types::v1::PackageManager1Package>(),
                                           false,
                                           *pm,
                                           *repo);

    // two apps, id1 and id2 are installed locally
    EXPECT_CALL(*repo, listLocalApps())
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{
          .arch = std::vector<std::string>{ "x86_64" },
          .base = "base",
          .channel = "main",
          .id = "id1",
          .kind = "app",
          .packageInfoV2Module = "binary",
          .runtime = "runtime",
          .version = "1.0.0",
        },
        api::types::v1::PackageInfoV2{
          .arch = std::vector<std::string>{ "x86_64" },
          .base = "base",
          .channel = "main",
          .id = "id2",
          .kind = "app",
          .packageInfoV2Module = "binary",
          .runtime = "runtime",
          .version = "1.0.0",
        },
      }));

    auto res = action->prepare();
    ASSERT_TRUE(res.has_value());

    // id1 has no update
    EXPECT_CALL(*repo,
                latestRemoteReference(AllOf(Field(&package::FuzzyReference::id, "id1"),
                                            Field(&package::FuzzyReference::channel, "main"))))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo" },
        .reference = package::Reference::parse("main:id1/1.0.0/x86_64").value() }));

    // id2 has update
    EXPECT_CALL(*repo,
                latestRemoteReference(AllOf(Field(&package::FuzzyReference::id, "id2"),
                                            Field(&package::FuzzyReference::channel, "main"))))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo" },
        .reference = package::Reference::parse("main:id2/1.1.0/x86_64").value() }));

    // base has update
    EXPECT_CALL(*repo,
                latestRemoteReference(AllOf(Field(&package::FuzzyReference::id, "base"),
                                            Field(&package::FuzzyReference::channel, "main"))))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo" },
        .reference = package::Reference::parse("main:base/1.0.1/x86_64").value() }));

    EXPECT_CALL(*repo,
                clearReference(AllOf(Field(&package::FuzzyReference::id, "base"),
                                     Field(&package::FuzzyReference::channel, "main")),
                               _,
                               _,
                               _))
      .WillOnce(Return(package::Reference::parse("main:base/1.0.0/x86_64").value()))
      .WillOnce(Return(package::Reference::parse("main:base/1.0.1/x86_64").value()));

    EXPECT_CALL(*repo,
                latestRemoteReference(AllOf(Field(&package::FuzzyReference::id, "runtime"),
                                            Field(&package::FuzzyReference::channel, "main"))))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo" },
        .reference = package::Reference::parse("main:runtime/1.0.0/x86_64").value() }));

    EXPECT_CALL(*repo,
                clearReference(AllOf(Field(&package::FuzzyReference::id, "runtime"),
                                     Field(&package::FuzzyReference::channel, "main")),
                               _,
                               _,
                               _))
      .WillOnce(Return(package::Reference::parse("main:runtime/1.0.0/x86_64").value()))
      .WillOnce(Return(package::Reference::parse("main:runtime/1.0.0/x86_64").value()));

    EXPECT_CALL(*repo,
                latestRemoteReference(AllOf(Field(&package::FuzzyReference::id, "extension"),
                                            Field(&package::FuzzyReference::channel, "main"))))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo" },
        .reference = package::Reference::parse("main:extension/1.0.0/x86_64").value() }));

    EXPECT_CALL(*repo,
                clearReference(AllOf(Field(&package::FuzzyReference::id, "extension"),
                                     Field(&package::FuzzyReference::channel, "main")),
                               _,
                               _,
                               _))
      .WillOnce(Return(package::Reference::parse("main:extension/1.0.0/x86_64").value()))
      .WillOnce(Return(package::Reference::parse("main:extension/1.0.0/x86_64").value()));

    EXPECT_CALL(*repo, getModuleList(_))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop", "other1" }));

    EXPECT_CALL(*repo, getRemoteModuleList(_, _))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop", "other2" }));

    EXPECT_CALL(*pm, installRef(_, _, IsSubsetOf(std::vector<std::string>{ "binary", "develop" })))
      .WillOnce(Return(utils::error::Result<void>{}))
      .WillOnce(Return(utils::error::Result<void>{}));

    auto baseItem =
      api::types::v1::RepositoryCacheLayersItem{ .info = api::types::v1::PackageInfoV2{
                                                   .arch = std::vector<std::string>{ "x86_64" },
                                                   .base = "base",
                                                   .channel = "main",
                                                   .id = "base",
                                                   .kind = "base",
                                                   .packageInfoV2Module = "binary",
                                                   .runtime = "runtime",
                                                   .version = "1.0.0",
                                                 } };
    EXPECT_CALL(*repo,
                getLayerItem(AllOf(Field(&package::Reference::id, "base"),
                                   Field(&package::Reference::channel, "main")),
                             _,
                             _))
      .WillOnce(Return(baseItem))
      .WillOnce(Return(baseItem));

    auto runtimeItem =
      api::types::v1::RepositoryCacheLayersItem{ .info = api::types::v1::PackageInfoV2{
                                                   .arch = std::vector<std::string>{ "x86_64" },
                                                   .base = "base",
                                                   .channel = "main",
                                                   .extensions =
                                                     std::vector<api::types::v1::ExtensionDefine>{
                                                       api::types::v1::ExtensionDefine{
                                                         .name = "extension",
                                                         .version = "1.0.0",
                                                       },
                                                     },
                                                   .id = "runtime",
                                                   .kind = "base",
                                                   .packageInfoV2Module = "binary",
                                                   .runtime = "runtime",
                                                   .version = "1.0.0",
                                                 } };

    EXPECT_CALL(*repo,
                getLayerItem(AllOf(Field(&package::Reference::id, "runtime"),
                                   Field(&package::Reference::channel, "main")),
                             _,
                             _))
      .WillOnce(Return(runtimeItem))
      .WillOnce(Return(runtimeItem));

    auto appId2Item =
      api::types::v1::RepositoryCacheLayersItem{ .info = api::types::v1::PackageInfoV2{
                                                   .arch = std::vector<std::string>{ "x86_64" },
                                                   .base = "base",
                                                   .channel = "main",
                                                   .id = "id2",
                                                   .kind = "app",
                                                   .packageInfoV2Module = "binary",
                                                   .runtime = "runtime",
                                                   .version = "1.0.0",
                                                 } };
    EXPECT_CALL(*repo,
                getLayerItem(AllOf(Field(&package::Reference::id, "id2"),
                                   Field(&package::Reference::channel, "main")),
                             _,
                             _))
      .WillOnce(Return(appId2Item));

    EXPECT_CALL(
      *pm,
      switchAppVersion(
        AllOf(Field(&package::Reference::id, "id2"),
              Field(&package::Reference::channel, "main"),
              Field(&package::Reference::version, package::Version::parse("1.0.0").value())),
        AllOf(Field(&package::Reference::id, "id2"),
              Field(&package::Reference::channel, "main"),
              Field(&package::Reference::version, package::Version::parse("1.1.0").value())),
        true))
      .WillOnce(Return(utils::error::Result<void>{}));

    service::PackageTask task({});
    res = action->doAction(task);
    if (!res) {
        LogE("failed to update app {}", res.error());
    }
    ASSERT_TRUE(res.has_value());
}

} // namespace
