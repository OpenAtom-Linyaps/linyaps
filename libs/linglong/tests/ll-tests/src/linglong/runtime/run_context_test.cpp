// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/run_context.h"

using namespace linglong;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

namespace {

using RunContext = linglong::runtime::RunContext;
using RuntimeLayer = linglong::runtime::RuntimeLayer;

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref,
                 std::string module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::LayerDir>,
                getMergedModuleDir,
                (const package::Reference &ref,
                 bool fallbackLayerDir,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::LayerDir>,
                createTempMergedModuleDir,
                (const package::Reference &ref, const std::vector<std::string> &modules),
                (override, const, noexcept));
};

class RuntimeLayerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        repo = std::make_unique<MockRepo>(tempDir->path());
    }

    void TearDown() override
    {
        tempDir.reset();
        repo.reset();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<MockRepo> repo;
};

TEST_F(RuntimeLayerTest, layerExist)
{
    // Create a valid reference
    auto ref = package::Reference::parse("stable:org.example.test/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << "Failed to create reference: " << ref.error().message();

    // Mock successful layer item retrieval
    api::types::v1::RepositoryCacheLayersItem mockItem;
    mockItem.info.id = "org.example.test";
    mockItem.info.version = "1.0.0";
    mockItem.info.kind = "app";
    mockItem.info.channel = "stable";
    mockItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(mockItem));

    // Create runtime layer
    RunContext context(*this->repo);
    auto layer = RuntimeLayer::create(*ref, context);
    ASSERT_TRUE(layer.has_value()) << "Failed to create runtime layer: " << layer.error().message();
}

TEST_F(RuntimeLayerTest, layerNotExist)
{
    LINGLONG_TRACE("layerNotExist");

    // Create a valid reference
    auto ref = package::Reference::parse("stable:org.example.nonexistent/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << "Failed to create reference: " << ref.error().message();

    // Mock failed layer item retrieval (layer doesn't exist)
    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(LINGLONG_ERR("Layer not found")));

    // Attempt to create runtime layer
    RunContext context(*this->repo);
    auto layer = RuntimeLayer::create(*ref, context);
    EXPECT_FALSE(layer.has_value()) << "Expected layer creation to fail for non-existent layer";
    EXPECT_TRUE(layer.error().message().find("failed to create runtime layer")
                != std::string::npos);
}

TEST_F(RuntimeLayerTest, resolveBinaryModule)
{
    // Create a valid reference
    auto ref = package::Reference::parse("stable:org.example.binary/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << "Failed to create reference: " << ref.error().message();

    // Mock successful layer item retrieval
    api::types::v1::RepositoryCacheLayersItem mockItem;
    mockItem.info.id = "org.example.binary";
    mockItem.info.version = "1.0.0";
    mockItem.info.kind = "app";
    mockItem.info.channel = "stable";
    mockItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(mockItem));

    // Mock successful layer directory retrieval for binary module
    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(_, true, _)).WillOnce(Return(mockLayerDir));

    // Create runtime layer
    RunContext context(*this->repo);
    auto layer = RuntimeLayer::create(*ref, context);
    ASSERT_TRUE(layer.has_value()) << "Failed to create runtime layer: " << layer.error().message();

    // Test resolving binary module
    auto result = layer->resolveLayer({ "binary" });
    ASSERT_TRUE(result.has_value())
      << "Failed to resolve binary module: " << result.error().message();

    // Verify layer directory is set
    EXPECT_TRUE(layer->getLayerDir().has_value());
}

TEST_F(RuntimeLayerTest, resolveMultiModules)
{
    // Create a valid reference
    auto ref = package::Reference::parse("stable:org.example.multi/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << "Failed to create reference: " << ref.error().message();

    // Mock successful layer item retrieval
    api::types::v1::RepositoryCacheLayersItem mockItem;
    mockItem.info.id = "org.example.multi";
    mockItem.info.version = "1.0.0";
    mockItem.info.kind = "app";
    mockItem.info.channel = "stable";
    mockItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(mockItem));

    // Mock successful merged module directory retrieval for multiple modules
    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, createTempMergedModuleDir(_, std::vector<std::string>{ "binary", "debug" }))
      .WillOnce(Return(mockLayerDir));

    // Create runtime layer
    RunContext context(*this->repo);
    auto layer = RuntimeLayer::create(*ref, context);
    ASSERT_TRUE(layer.has_value()) << "Failed to create runtime layer: " << layer.error().message();

    // Test resolving multiple modules
    auto result = layer->resolveLayer({ "binary", "debug" });
    ASSERT_TRUE(result.has_value())
      << "Failed to resolve multiple modules: " << result.error().message();

    // Verify layer directory is set
    EXPECT_TRUE(layer->getLayerDir().has_value());
}

TEST_F(RuntimeLayerTest, resolveSubRef)
{
    // Create a valid reference
    auto ref = package::Reference::parse("stable:org.example.subref/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << "Failed to create reference: " << ref.error().message();

    // Mock successful layer item retrieval
    api::types::v1::RepositoryCacheLayersItem mockItem;
    mockItem.info.id = "org.example.subref";
    mockItem.info.version = "1.0.0";
    mockItem.info.kind = "app";
    mockItem.info.channel = "stable";
    mockItem.info.arch = { std::string{ "x86_64" } };
    mockItem.info.uuid = "test-uuid-12345";

    EXPECT_CALL(*repo, getLayerItem(_, _, _)).WillOnce(Return(mockItem));

    // Mock successful layer directory retrieval with sub-reference
    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo,
                getMergedModuleDir(_, "binary", std::optional<std::string>("test-uuid-12345")))
      .WillOnce(Return(mockLayerDir));

    // Create runtime layer
    RunContext context(*this->repo);
    auto layer = RuntimeLayer::create(*ref, context);
    ASSERT_TRUE(layer.has_value()) << "Failed to create runtime layer: " << layer.error().message();

    // Test resolving with sub-reference
    auto result = layer->resolveLayer({}, "test-uuid-12345");
    ASSERT_TRUE(result.has_value())
      << "Failed to resolve layer with sub-ref: " << result.error().message();

    // Verify layer directory is set
    EXPECT_TRUE(layer->getLayerDir().has_value());
}

} // namespace
