/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/package/architecture.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/uab_installation.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

using namespace linglong;
using linglong::service::UabInstallationAction;
using ::testing::_;
using ::testing::Return;

class MockOSTreeRepo : public repo::OSTreeRepo
{
public:
    MockOSTreeRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReference,
                (const package::FuzzyReference &fuzzyRef,
                 const repo::clearReferenceOption &option,
                 const std::string &module,
                 const std::optional<std::string> &repo),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<package::Reference>,
                latestLocalReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));
};

namespace {
api::types::v1::UabLayer create_layer(const std::string &id,
                                      const std::string &version,
                                      const std::string &channel,
                                      const std::string &kind)
{
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();
    api::types::v1::UabLayer layer;
    layer.minified = false;
    layer.info.id = id;
    layer.info.version = version;
    layer.info.channel = channel;
    layer.info.kind = kind;
    layer.info.arch = { currentArch };
    return layer;
}
} // namespace

TEST(CheckUABLayersConstrain, EmptyLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    EXPECT_CALL(mockRepo, clearReference(_, _, _, _)).Times(0);
    std::vector<api::types::v1::UabLayer> layers;
    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckUABLayersConstrain, ArchMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    api::types::v1::UabLayer layer;
    // An arch that doesn't match current arch
    layer.minified = false;
    layer.info.arch = { "non-existent-arch" };
    layers.push_back(layer);

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, DifferentIds)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layers.push_back(layer1);

    api::types::v1::UabLayer layer2;
    layer2.minified = false;
    layer2.info.arch = { currentArch };
    layer2.info.id = "id2";
    layers.push_back(layer2);

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, DifferentVersions)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layer1.info.version = "1.0.0";
    layers.push_back(layer1);

    api::types::v1::UabLayer layer2;
    layer2.minified = false;
    layer2.info.arch = { currentArch };
    layer2.info.id = "id1";
    layer2.info.version = "2.0.0";
    layers.push_back(layer2);

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, ExtraModuleNoBinaryAndNotInstalled)
{
    LINGLONG_TRACE("ExtraModuleNoBinaryAndNotInstalled");

    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.channel = "main";
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layer1.info.version = "1.0.0";
    layer1.info.packageInfoV2Module = "extra";
    layers.push_back(layer1);

    EXPECT_CALL(mockRepo, clearReference(_, _, _, _)).WillOnce(Return(LINGLONG_ERR("")));

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, ExtraModuleNoBinaryAndWrongVersionInstalled)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.channel = "main";
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layer1.info.version = "1.0.0";
    layer1.info.packageInfoV2Module = "extra";
    layers.push_back(layer1);

    auto localRef = package::Reference::parse("main:id1/2.0.0/" + currentArch);
    EXPECT_TRUE(localRef.has_value());

    EXPECT_CALL(mockRepo, clearReference(_, _, _, _)).WillOnce(Return(*localRef));

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, ExtraModuleNoBinaryButInstalled)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.channel = "main";
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layer1.info.version = "1.0.0";
    layer1.info.packageInfoV2Module = "extra";
    layers.push_back(layer1);

    auto localRef = package::Reference::parse("main:id1/1.0.0/" + currentArch);
    EXPECT_TRUE(localRef.has_value());

    EXPECT_CALL(mockRepo, clearReference(_, _, _, _)).WillOnce(Return(*localRef));

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckUABLayersConstrain, ExtraModuleWithBinary)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();

    api::types::v1::UabLayer layer1;
    layer1.minified = false;
    layer1.info.channel = "main";
    layer1.info.arch = { currentArch };
    layer1.info.id = "id1";
    layer1.info.version = "1.0.0";
    layer1.info.packageInfoV2Module = "extra";
    layers.push_back(layer1);

    api::types::v1::UabLayer layer2;
    layer2.minified = false;
    layer2.info.channel = "main";
    layer2.info.arch = { currentArch };
    layer2.info.id = "id1";
    layer2.info.version = "1.0.0";
    layer2.info.packageInfoV2Module = "binary";
    layers.push_back(layer2);

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_TRUE(result.has_value());
}

// checkExecModeUABLayers tests
TEST(CheckExecModeUABLayers, NoAppLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "runtime")
    };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerNoRuntime)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.packageInfoV2Module = "binary";
    std::vector<api::types::v1::UabLayer> layers = { layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerWithRuntimeNoRuntimeLayer)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.runtime = "runtime.id/1.0.0";
    std::vector<api::types::v1::UabLayer> layers = { layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerWithRuntimeIdMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto app_layer = create_layer("id1", "1.0.0", "main", "app");
    app_layer.info.runtime = "runtime.id/1.0.0";
    auto runtime_layer = create_layer("runtime.id_mismatch", "1.0.0", "main", "runtime");
    std::vector<api::types::v1::UabLayer> layers = { app_layer, runtime_layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerWithRuntimeChannelMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto app_layer = create_layer("id1", "1.0.0", "main", "app");
    app_layer.info.runtime = "runtime.id/1.0.0";
    auto runtime_layer = create_layer("runtime.id", "1.0.0", "main_mismatch", "runtime");
    std::vector<api::types::v1::UabLayer> layers = { app_layer, runtime_layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerWithRuntimeVersionMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto app_layer = create_layer("id1", "1.0.0", "main", "app");
    app_layer.info.runtime = "runtime.id/2.0.0";
    auto runtime_layer = create_layer("runtime.id", "1.0.0", "main", "runtime");
    std::vector<api::types::v1::UabLayer> layers = { app_layer, runtime_layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, AppLayerWithRuntimeOk)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto app_layer = create_layer("id1", "1.0.0", "main", "app");
    app_layer.info.runtime = "runtime.id/1.0";
    app_layer.info.packageInfoV2Module = "binary";
    auto runtime_layer = create_layer("runtime.id", "1.0.1", "main", "runtime");
    runtime_layer.info.packageInfoV2Module = "runtime";
    std::vector<api::types::v1::UabLayer> layers = { app_layer, runtime_layer };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckExecModeUABLayers, ConstrainFailOnAppLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = { create_layer("id1", "1.0.0", "main", "app"),
                                                     create_layer("id2", "1.0.0", "main", "app") };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckExecModeUABLayers, ConstrainFailOnOtherLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto app_layer = create_layer("app_id", "1.0.0", "main", "app");
    app_layer.info.runtime = "main:runtime.id/1.0.0";
    app_layer.info.packageInfoV2Module = "binary";

    auto runtime_layer1 = create_layer("runtime.id", "1.0.0", "main", "runtime");
    runtime_layer1.info.packageInfoV2Module = "runtime";
    auto runtime_layer2 =
      create_layer("runtime.id", "2.0.0", "main", "runtime"); // different version
    runtime_layer2.info.packageInfoV2Module = "runtime";

    std::vector<api::types::v1::UabLayer> layers = { app_layer, runtime_layer1, runtime_layer2 };
    auto result = UabInstallationAction::checkExecModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

// checkDistributionModeUABLayers tests
TEST(CheckDistributionModeUABLayers, NoLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers;
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckDistributionModeUABLayers, BothAppAndOtherLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app"),
        create_layer("id1", "1.0.0", "main", "runtime")
    };
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckDistributionModeUABLayers, OnlyAppLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.packageInfoV2Module = "binary";
    std::vector<api::types::v1::UabLayer> layers = { layer };
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckDistributionModeUABLayers, OnlyOtherLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto layer = create_layer("id1", "1.0.0", "main", "runtime");
    layer.info.packageInfoV2Module = "runtime";
    std::vector<api::types::v1::UabLayer> layers = { layer };
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(CheckDistributionModeUABLayers, ConstrainFailOnAppLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = { create_layer("id1", "1.0.0", "main", "app"),
                                                     create_layer("id2", "1.0.0", "main", "app") };
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckDistributionModeUABLayers, ConstrainFailOnOtherLayers)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "runtime"),
        create_layer("id1", "2.0.0", "main", "runtime")
    };
    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    EXPECT_FALSE(result.has_value());
}
