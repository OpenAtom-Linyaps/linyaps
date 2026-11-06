/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
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
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReferenceLocal,
                (const package::FuzzyReference &fuzzyRef, bool semanticMatching),
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
    EXPECT_CALL(mockRepo, clearReferenceLocal(_, _)).Times(0);
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

    EXPECT_CALL(mockRepo, clearReferenceLocal(_, _)).WillOnce(Return(LINGLONG_ERR("")));

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

    EXPECT_CALL(mockRepo, clearReferenceLocal(_, _)).WillOnce(Return(*localRef));

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

    EXPECT_CALL(mockRepo, clearReferenceLocal(_, _)).WillOnce(Return(*localRef));

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

TEST(UABLayerModuleValidation, AcceptsSinglePathComponents)
{
    for (const auto *module : { "binary", "runtime", "develop", "extra.module" }) {
        auto result = UabInstallationAction::validateUABLayerModule(module);
        EXPECT_TRUE(result.has_value()) << "module was rejected: " << module;
    }
}

TEST(UABLayerModuleValidation, RejectsUnsafePathValues)
{
    for (const auto *module : { "", ".", "..", "../x", "module/subdir", "/tmp/layer", "a\\b" }) {
        auto result = UabInstallationAction::validateUABLayerModule(module);
        EXPECT_FALSE(result.has_value()) << "module was accepted: " << module;
    }
}

TEST(UABLayerModuleValidation, RejectsEmbeddedNul)
{
    // std::string keeps the trailing bytes, but a later C API call would stop at the NUL.
    const std::string module("a\0b", 3);
    auto result = UabInstallationAction::validateUABLayerModule(module);
    EXPECT_FALSE(result.has_value());
}

TEST(UABLayerIdValidation, AcceptsSinglePathComponents)
{
    for (const auto *id : { "org.example.app", "id1", "a_b-c" }) {
        auto result = UabInstallationAction::validateUABLayerId(id);
        EXPECT_TRUE(result.has_value()) << "id was rejected: " << id;
    }
}

TEST(UABLayerIdValidation, RejectsUnsafePathValues)
{
    for (const auto *id : { "", ".", "..", "../x", "id/subdir", "/tmp/id", "a\\b" }) {
        auto result = UabInstallationAction::validateUABLayerId(id);
        EXPECT_FALSE(result.has_value()) << "id was accepted: " << id;
    }

    const std::string withNul("a\0b", 3);
    EXPECT_FALSE(UabInstallationAction::validateUABLayerId(withNul).has_value());
}

TEST(UABLayerPathValidation, AcceptsPathUnderLayersRoot)
{
    TempDir tempDir;
    const auto layersRoot = tempDir.path() / "layers";
    const auto layerDir = layersRoot / "org.example.app" / "binary";

    auto result = UabInstallationAction::validateUABLayerPath(layersRoot, layerDir);
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST(UABLayerPathValidation, RejectsDotDotTraversal)
{
    TempDir tempDir;
    const auto layersRoot = tempDir.path() / "layers";
    const auto layerDir = layersRoot / "org.example.app" / ".." / ".." / "outside";

    auto result = UabInstallationAction::validateUABLayerPath(layersRoot, layerDir);
    EXPECT_FALSE(result.has_value());
}

TEST(UABLayerPathValidation, RejectsAbsoluteEscape)
{
    TempDir tempDir;
    const auto layersRoot = tempDir.path() / "layers";

    auto result = UabInstallationAction::validateUABLayerPath(layersRoot, "/etc");
    EXPECT_FALSE(result.has_value());
}

TEST(UABLayerPathValidation, RejectsSiblingWithCommonPrefix)
{
    TempDir tempDir;
    const auto layersRoot = tempDir.path() / "layers";
    // "layers-escape" shares the textual prefix "layers" but is not inside it.
    const auto layerDir = tempDir.path() / "layers-escape" / "id" / "binary";

    auto result = UabInstallationAction::validateUABLayerPath(layersRoot, layerDir);
    EXPECT_FALSE(result.has_value());
}

TEST(UABLayerPathValidation, RejectsSymlinkEscape)
{
    TempDir tempDir;
    std::error_code ec;
    const auto layersRoot = tempDir.path() / "layers";
    const auto outside = tempDir.path() / "outside";
    std::filesystem::create_directories(layersRoot, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::create_directories(outside, ec);
    ASSERT_FALSE(ec) << ec.message();

    const auto link = layersRoot / "org.example.app";
    std::filesystem::create_directory_symlink(outside, link, ec);
    if (ec) {
        GTEST_SKIP() << "cannot create directory symlink: " << ec.message();
    }

    auto result = UabInstallationAction::validateUABLayerPath(layersRoot, link / "binary");
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
