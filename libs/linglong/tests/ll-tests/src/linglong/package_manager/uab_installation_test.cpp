/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/scoped_runtime_paths.h"
#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/UabLayer.hpp"
#include "linglong/package/architecture.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/package/uab_packager.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/package_manager/uab_installation.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <fstream>

#include <fcntl.h>
#include <unistd.h>

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

namespace {

using linglong::api::types::v1::CommonOptions;
using linglong::api::types::v1::PackageInfoV2;

class UabInstallationFlowTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        staging = std::make_unique<test::ScopedStagingDir>();
        if (!staging->ok()) {
            GTEST_SKIP() << staging->error();
        }

        tempDir = std::make_unique<TempDir>();
        repoRoot = tempDir->path() / "repo";
        ASSERT_TRUE(std::filesystem::create_directories(repoRoot));

        auto created = repo::OSTreeRepo::create(
          repoRoot,
          api::types::v1::RepoConfigV2{
            .defaultRepo = "stable",
            .repos = { api::types::v1::Repo{ .name = "stable",
                                             .url = "https://example.com/repo" } },
            .version = 2,
          });
        ASSERT_TRUE(created) << created.error().message();
        repo = created->get();
        packageManager =
          std::make_unique<service::PackageManager>(std::move(*created), nullptr, nullptr);
    }

    std::filesystem::path createLayer(const std::string &id,
                                      const std::string &version,
                                      const std::string &kind,
                                      const std::string &module,
                                      const std::optional<std::string> &base = std::nullopt)
    {
        auto layerDir = tempDir->path() / "layers" / (id + "-" + module);
        std::error_code ec;
        std::filesystem::create_directories(layerDir / "files" / "usr" / "bin", ec);
        EXPECT_FALSE(ec) << ec.message();
        std::ofstream{ layerDir / "files" / "usr" / "bin" / id } << id;

        PackageInfoV2 info;
        info.id = id;
        info.version = version;
        info.channel = "main";
        info.kind = kind;
        info.packageInfoV2Module = module;
        info.arch = { package::Architecture::currentCPUArchitecture().toString() };
        if (base) {
            info.base = *base;
        }
        std::ofstream{ layerDir / "info.json" } << nlohmann::json(info).dump();
        return layerDir;
    }

    std::filesystem::path createUab(const std::string &name,
                                    const std::vector<std::filesystem::path> &layerDirs)
    {
        auto header = tempDir->path() / "uab-header";
        std::filesystem::copy_file("/proc/self/exe", header);

        auto buildDir = tempDir->path() / ("build-" + name);
        std::filesystem::create_directories(buildDir);
        package::UABPackager packager(buildDir);
        packager.setDefaultHeader(header);
        packager.setCompressor("lz4");
        for (const auto &layerDir : layerDirs) {
            auto appended = packager.appendLayer(package::LayerDir{ layerDir });
            EXPECT_TRUE(appended) << appended.error().message();
        }

        auto output = tempDir->path() / (name + ".uab");
        auto packed = packager.pack(output, package::UABPackagerMode::Distribution);
        EXPECT_TRUE(packed) << packed.error().message();
        return output;
    }

    utils::error::Result<void> runAction(const std::filesystem::path &uabPath,
                                         service::PackageTask &task,
                                         CommonOptions options = {})
    {
        LINGLONG_TRACE("run uab installation action");
        int fd = ::open(uabPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd == -1) {
            return LINGLONG_ERR("failed to open uab file");
        }
        auto closeFd = linglong::utils::finally::finally([fd] {
            ::close(fd);
        });

        auto action = UabInstallationAction::create(fd, *packageManager, *repo, options);
        if (!action) {
            return LINGLONG_ERR("failed to create uab action");
        }

        auto prepared = action->prepare();
        if (!prepared) {
            return prepared;
        }

        return action->doAction(task);
    }

    std::unique_ptr<test::ScopedStagingDir> staging;
    std::unique_ptr<TempDir> tempDir;
    std::filesystem::path repoRoot;
    repo::OSTreeRepo *repo{ nullptr };
    std::unique_ptr<service::PackageManager> packageManager;
};

TEST_F(UabInstallationFlowTest, InstallsDistributionUabWithBaseLayer)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }

    auto baseLayer = createLayer("org.deepin.base", "1.0.0", "base", "binary");
    auto uab = createUab("base-only", { baseLayer });

    service::PackageTask task([](service::Task &) { }, nullptr);
    auto result = runAction(uab, task);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(task.state(), api::types::v1::State::Succeed);

    auto installed = repo->listLocalBy(repo::repoCacheQuery{ .id = "org.deepin.base" });
    ASSERT_TRUE(installed) << installed.error().message();
    ASSERT_EQ(installed->size(), 1);
    EXPECT_EQ(installed->front().info.packageInfoV2Module, "binary");
    EXPECT_TRUE(std::filesystem::exists(
      repo
        ->getLayerDir(
          package::Reference::parse("main:org.deepin.base/1.0.0/"
                                    + package::Architecture::currentCPUArchitecture().toString())
            .value(),
          "binary")
        .value()
        .path()
      / "info.json"));
}

TEST_F(UabInstallationFlowTest, InstallsAppUabWhenBaseIsAlreadyInstalled)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }

    auto baseLayer = createLayer("org.deepin.base", "1.0.0", "base", "binary");
    auto imported = repo->importLayerDir(package::LayerDir{ baseLayer });
    ASSERT_TRUE(imported) << imported.error().message();

    const auto baseRef =
      "org.deepin.base/1.0.0/" + package::Architecture::currentCPUArchitecture().toString();
    auto appLayer = createLayer("org.example.app", "1.0.0", "app", "binary", baseRef);
    auto uab = createUab("app-only", { appLayer });

    service::PackageTask task([](service::Task &) { }, nullptr);
    auto result = runAction(uab, task);

    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(task.state(), api::types::v1::State::Succeed);

    auto installed = repo->listLocalBy(repo::repoCacheQuery{ .id = "org.example.app" });
    ASSERT_TRUE(installed) << installed.error().message();
    ASSERT_EQ(installed->size(), 1);
    EXPECT_EQ(installed->front().info.kind, "app");
}

TEST_F(UabInstallationFlowTest, RejectsMalformedUabBeforeInstallingAnything)
{
    auto malformed = tempDir->path() / "malformed.uab";
    std::ofstream{ malformed } << "not an elf file";

    service::PackageTask task([](service::Task &) { }, nullptr);
    auto result = runAction(malformed, task);

    EXPECT_FALSE(result.has_value());
    auto installed = repo->listLocal();
    ASSERT_TRUE(installed) << installed.error().message();
    EXPECT_TRUE(installed->empty());
}

} // namespace
