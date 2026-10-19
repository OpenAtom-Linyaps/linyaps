/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
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
using ::testing::HasSubstr;
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
                                      const std::string &kind,
                                      const std::string &module = "binary")
{
    auto currentArch = package::Architecture::currentCPUArchitecture().toString();
    api::types::v1::UabLayer layer;
    layer.minified = false;
    layer.info.id = id;
    layer.info.version = version;
    layer.info.channel = channel;
    layer.info.kind = kind;
    layer.info.arch = { currentArch };
    layer.info.packageInfoV2Module = module;
    return layer;
}

std::string different_architecture()
{
    const auto current = package::Architecture::currentCPUArchitecture().toString();
    for (const auto &candidate :
         { "x86_64", "arm64", "loongarch64", "loong64", "sw64", "riscv64", "mips64" }) {
        auto arch = package::Architecture::parse(candidate);
        if (arch && arch->toString() != current) {
            return candidate;
        }
    }
    return {};
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
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    // An arch that doesn't match current arch
    layer.info.arch = { "non-existent-arch" };
    std::vector<api::types::v1::UabLayer> layers{ layer };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, DifferentIds)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app"),
        create_layer("id2", "1.0.0", "main", "app"),
    };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_FALSE(result.has_value());
}

TEST(CheckUABLayersConstrain, DifferentVersions)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app"),
        create_layer("id1", "2.0.0", "main", "app"),
    };

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
    layer1.info.kind = "app";
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
    layer1.info.kind = "app";
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
    layer1.info.kind = "app";
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
    layer1.info.kind = "app";
    layer1.info.version = "1.0.0";
    layer1.info.packageInfoV2Module = "extra";
    layers.push_back(layer1);

    api::types::v1::UabLayer layer2;
    layer2.minified = false;
    layer2.info.channel = "main";
    layer2.info.arch = { currentArch };
    layer2.info.id = "id1";
    layer2.info.kind = "app";
    layer2.info.version = "1.0.0";
    layer2.info.packageInfoV2Module = "binary";
    layers.push_back(layer2);

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    EXPECT_TRUE(result.has_value());
}

TEST(ValidateUabLayer, AcceptsCompleteMetadata)
{
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    auto result = UabInstallationAction::validateUabLayer(layer);
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST(ValidateUabLayer, RejectsEmptyArchitecture)
{
    // The original crash path indexed arch[0] before checking whether the
    // generated deserializer had produced an empty vector.
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch.clear();

    auto result = UabInstallationAction::validateUabLayer(layer);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}

TEST(ValidateUabLayer, RejectsUnparsableArchitecture)
{
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch = { "not-an-architecture" };

    auto result = UabInstallationAction::validateUabLayer(layer);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("unknown architecture"));
}

TEST(ValidateUabLayer, RejectsArchitectureThatDoesNotMatchHost)
{
    const auto mismatchedArch = different_architecture();
    ASSERT_FALSE(mismatchedArch.empty()) << "no alternative architecture is available";

    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch = { mismatchedArch };

    auto result = UabInstallationAction::validateUabLayer(layer);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("not match host architecture"));
}

TEST(ValidateUabLayer, RejectsInvalidSecondaryArchitecture)
{
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch.push_back("not-an-architecture");

    auto result = UabInstallationAction::validateUabLayer(layer);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateUabLayer, RejectsSecondaryArchitectureThatDoesNotMatchHost)
{
    const auto mismatchedArch = different_architecture();
    ASSERT_FALSE(mismatchedArch.empty()) << "no alternative architecture is available";

    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch.push_back(mismatchedArch);

    auto result = UabInstallationAction::validateUabLayer(layer);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("not match host architecture"));
}

TEST(ValidateUabLayer, RejectsEmptyOrUnparsableMetadataFields)
{
    using Mutator = void (*)(api::types::v1::UabLayer &);

    struct TestCase
    {
        const char *name;
        Mutator mutate;
        const char *expectedMessage;
    };

    const TestCase cases[] = {
        { "id",
          [](api::types::v1::UabLayer &layer) {
              layer.info.id.clear();
          },
          "missing id" },
        { "channel",
          [](api::types::v1::UabLayer &layer) {
              layer.info.channel.clear();
          },
          "missing channel" },
        { "empty version",
          [](api::types::v1::UabLayer &layer) {
              layer.info.version.clear();
          },
          "missing version" },
        { "unparsable version",
          [](api::types::v1::UabLayer &layer) {
              layer.info.version = ".";
          },
          "parse version failed" },
        { "kind",
          [](api::types::v1::UabLayer &layer) {
              layer.info.kind.clear();
          },
          "missing kind" },
        { "module",
          [](api::types::v1::UabLayer &layer) {
              layer.info.packageInfoV2Module.clear();
          },
          "missing module" },
    };

    for (const auto &testCase : cases) {
        SCOPED_TRACE(testCase.name);
        auto layer = create_layer("id1", "1.0.0", "main", "app");
        testCase.mutate(layer);

        auto result = UabInstallationAction::validateUabLayer(layer);
        ASSERT_FALSE(result.has_value());
        EXPECT_THAT(result.error().message(), HasSubstr(testCase.expectedMessage));
    }
}

TEST(CheckUABLayersConstrain, EmptyArchitectureInFirstLayer)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    auto layer = create_layer("id1", "1.0.0", "main", "app");
    layer.info.arch.clear();

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, { layer });
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}

TEST(CheckUABLayersConstrain, EmptyArchitectureInLaterLayer)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "app", "develop"),
    };
    layers.back().info.arch.clear();

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}

TEST(CheckUABLayersConstrain, InvalidArchitectureInLaterLayer)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "app", "develop"),
    };
    layers.back().info.arch = { "not-an-architecture" };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("unknown architecture"));
}

TEST(CheckUABLayersConstrain, ChannelMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "beta", "app", "develop"),
    };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("different channel"));
}

TEST(CheckUABLayersConstrain, KindMismatch)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "runtime", "runtime"),
    };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("different kind"));
}

TEST(CheckUABLayersConstrain, DuplicateModule)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "app", "binary"),
    };

    auto result = UabInstallationAction::checkUABLayersConstrain(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("duplicate module"));
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

TEST(CheckDistributionModeUABLayers, RejectsEmptyArchitectureInAppLayerBeforeSplitting)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "runtime", "runtime"),
    };
    layers.front().info.arch.clear();

    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}

TEST(CheckDistributionModeUABLayers, RejectsEmptyArchitectureInOtherLayerBeforeSplitting)
{
    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;
    std::vector<api::types::v1::UabLayer> layers = {
        create_layer("id1", "1.0.0", "main", "app", "binary"),
        create_layer("id1", "1.0.0", "main", "runtime", "runtime"),
    };
    layers.back().info.arch.clear();

    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, layers);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}

TEST(CheckDistributionModeUABLayers, RejectsEmptyArchitectureFromDeserializedMetadata)
{
    // PackageInfoV2::arch is generated as std::vector<std::string>, so the
    // deserializer happily accepts an empty JSON array. This mirrors the
    // metadata shape that reaches the installation action through linglong.meta.
    constexpr auto layerJson = R"json(
        {
            "info": {
                "arch": [],
                "base": "",
                "channel": "main",
                "id": "com.example.app",
                "kind": "app",
                "module": "binary",
                "name": "demo",
                "schema_version": "1",
                "size": 0,
                "version": "1.0.0"
            },
            "minified": false
        }
    )json";

    auto layer = nlohmann::json::parse(layerJson).get<api::types::v1::UabLayer>();
    ASSERT_TRUE(layer.info.arch.empty());

    TempDir tempDir;
    MockOSTreeRepo mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto result = UabInstallationAction::checkDistributionModeUABLayers(repo, { layer });
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), HasSubstr("missing architecture"));
}
