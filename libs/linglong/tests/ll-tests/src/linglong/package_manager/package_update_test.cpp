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
#include "linglong/package_manager/package_update.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "ocppi/cli/crun/Crun.hpp"

namespace {

using namespace linglong;
using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::IsSubsetOf;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace testdata {

api::types::v1::PackageInfoV2 baseV100{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "",
    .channel = "main",
    .id = "base",
    .kind = "base",
    .packageInfoV2Module = "binary",
    .name = "base",
    .version = "1.0.0",
};

api::types::v1::PackageInfoV2 baseV101{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "",
    .channel = "main",
    .id = "base",
    .kind = "base",
    .packageInfoV2Module = "binary",
    .name = "base",
    .version = "1.0.1",
};

api::types::v1::PackageInfoV2 runtimeV100{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "",
    .channel = "main",
    .extensions = std::vector<api::types::v1::ExtensionDefine>{ api::types::v1::ExtensionDefine{
      .directory = "/tmp", .name = "extension", .version = "1.0.0" } },
    .id = "runtime",
    .kind = "runtime",
    .packageInfoV2Module = "binary",
    .name = "runtime",
    .version = "1.0.0",
};

api::types::v1::PackageInfoV2 idV100{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "main:base/1.0.0/x86_64",
    .channel = "main",
    .id = "id1",
    .kind = "app",
    .packageInfoV2Module = "binary",
    .name = "id1",
    .runtime = "main:runtime/1.0.0/x86_64",
    .version = "1.0.0",
};

api::types::v1::PackageInfoV2 id2V100{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "main:base/1.0.0/x86_64",
    .channel = "main",
    .id = "id2",
    .kind = "app",
    .packageInfoV2Module = "binary",
    .name = "id2",
    .runtime = "main:runtime/1.0.0/x86_64",
    .version = "1.0.0",
};

api::types::v1::PackageInfoV2 id2V110{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "main:base/1.0.1/x86_64",
    .channel = "main",
    .id = "id2",
    .kind = "app",
    .packageInfoV2Module = "binary",
    .name = "id2",
    .runtime = "main:runtime/1.0.0/x86_64",
    .version = "1.1.0",
};

api::types::v1::PackageInfoV2 extension{
    .arch = std::vector<std::string>{ "x86_64" },
    .base = "main:base/1.0.1/x86_64",
    .channel = "main",
    .id = "extension",
    .kind = "extension",
    .packageInfoV2Module = "binary",
    .name = "extension",
    .runtime = "main:runtime/1.0.0/x86_64",
    .version = "1.0.0",
};

} // namespace testdata

class MockPackageManager : public service::PackageManager
{
public:
    MockPackageManager(repo::OSTreeRepo &repo, runtime::ContainerBuilder &builder, QObject *parent)
        : service::PackageManager(repo, builder, parent)
    {
    }

    MOCK_METHOD((utils::error::Result<
                  std::optional<std::pair<package::ReferenceWithRepo, std::vector<std::string>>>>),
                needToUpgrade,
                (const package::FuzzyReference &fuzzy,
                 std::optional<package::Reference> &local,
                 bool installIfMissing),
                (override));

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

    MOCK_METHOD(utils::error::Result<repo::RefMetaData>,
                fetchRefMetaData,
                (const package::ReferenceWithRepo &ref, const std::string &module, bool fetchInfo),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RefStatistics>,
                getRefStatistics,
                (const repo::RefMetaData &meta),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref,
                 std::string module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>, mergeModules, (), (override, const, noexcept));
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
                                           false,
                                           *pm,
                                           *repo);

    EXPECT_CALL(*repo, listLocalApps())
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        testdata::idV100,
        testdata::id2V100,
      }));

    auto res = action->prepare();
    ASSERT_TRUE(res.has_value());

    auto baseRef = package::Reference::fromPackageInfo(testdata::baseV101);
    ASSERT_TRUE(baseRef.has_value());

    auto runtimeRef = package::Reference::fromPackageInfo(testdata::runtimeV100);
    ASSERT_TRUE(runtimeRef.has_value());

    EXPECT_CALL(*pm, needToUpgrade(_, _, false))
      // id1: app has no updates
      .WillOnce(Return(std::nullopt))
      // id1: runtime's extension has no updates
      .WillOnce(Return(std::nullopt))
      // id2: app has updates
      .WillOnce(Return(std::make_pair(
        package::ReferenceWithRepo{ .repo = api::types::v1::Repo{ .name = "repo" },
                                    .reference =
                                      package::Reference::parse("main:id2/1.1.0/x86_64").value() },
        std::vector<std::string>{ "binary", "develop" })))
      // id2's dependencies: runtime's extension has no updates
      .WillOnce(Return(std::nullopt));

    EXPECT_CALL(*pm, needToUpgrade(_, _, true))
      // id1: base has updates
      .WillOnce(Return(std::make_pair(
        package::ReferenceWithRepo{ .repo = api::types::v1::Repo{ .name = "repo" },
                                    .reference =
                                      package::Reference::parse("main:base/1.0.1/x86_64").value() },
        std::vector<std::string>{ "binary" })))
      // id1: runtime has no updates
      .WillOnce(DoAll(SetArgReferee<1>(*runtimeRef), Return(std::nullopt)))
      // id2's dependencies: base has no updates
      .WillOnce(DoAll(SetArgReferee<1>(*baseRef), Return(std::nullopt)))
      // id2's dependencies: runtime has no updates
      .WillOnce(DoAll(SetArgReferee<1>(*runtimeRef), Return(std::nullopt)));

    EXPECT_CALL(*repo, fetchRefMetaData(_, "binary", true))
      .WillOnce(Return(repo::RefMetaData{ "rev1", nlohmann::json(testdata::baseV101).dump() }))
      .WillOnce(Return(repo::RefMetaData{ "rev2", nlohmann::json(testdata::id2V110).dump() }));
    EXPECT_CALL(*repo, fetchRefMetaData(_, "develop", false))
      .WillOnce(Return(repo::RefMetaData{ "rev3", nlohmann::json(testdata::id2V110).dump() }));

    EXPECT_CALL(*repo, getRefStatistics(_)).WillRepeatedly([](const repo::RefMetaData &) {
        return utils::error::Result<repo::RefStatistics>{
            repo::RefStatistics{ .archived = 1024, .needed_archived = 512 }
        };
    });

    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillOnce(Return(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = testdata::runtimeV100,
        } }))
      .WillOnce(Return(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = testdata::baseV101,
        } }))
      .WillOnce(Return(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = testdata::runtimeV100,
        } }));

    EXPECT_CALL(*pm, installRefModule(_, _, _))
      .WillRepeatedly([](service::Task &, const package::ReferenceWithRepo &, const std::string &) {
          return utils::error::Result<void>{};
      });

    EXPECT_CALL(*repo, mergeModules()).WillOnce([]() {
        return utils::error::Result<void>{};
    });

    EXPECT_CALL(*pm, switchAppVersion(_, _, true)).WillOnce(Return(utils::error::Result<void>{}));

    service::PackageTask task({});
    res = action->doAction(task);
    ASSERT_TRUE(res.has_value());
}

} // namespace
