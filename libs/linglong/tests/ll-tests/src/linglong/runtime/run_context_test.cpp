// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/oci-cfg-generators/container_cfg_builder.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/run_context.h"

#include <QCryptographicHash>
#include <QFile>

#include <fstream>

using namespace linglong;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::WithArg;

namespace {

using RunContext = linglong::runtime::RunContext;
using RuntimeLayer = linglong::runtime::RuntimeLayer;
using ResolveOptions = linglong::runtime::ResolveOptions;

std::string specChecksum(const std::filesystem::path &path)
{
    QFile file(QString::fromStdString(path.string()));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));

    QCryptographicHash hash(QCryptographicHash::Sha256);
    EXPECT_TRUE(hash.addData(&file));

    return hash.result().toHex().toStdString();
}

MATCHER_P(FuzzyRefIdEq, id, "")
{
    return arg.id == id;
}

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
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
    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReference,
                (const package::FuzzyReference &fuzzy,
                 const repo::clearReferenceOption &opts,
                 const std::string &module,
                 const std::optional<std::string> &repo),
                (override, const, noexcept));
};

class RunContextTest : public ::testing::Test
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

TEST_F(RunContextTest, layerExist)
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

TEST_F(RunContextTest, layerNotExist)
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

TEST_F(RunContextTest, resolveBinaryModule)
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

TEST_F(RunContextTest, resolveMultiModules)
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

TEST_F(RunContextTest, resolveSubRef)
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

TEST_F(RunContextTest, resolveRunnableWithApp)
{
    LINGLONG_TRACE("resolveRunnableWithApp");

    auto runnableRef = package::Reference::parse("stable:org.example.runnable/1.0.0/x86_64");
    ASSERT_TRUE(runnableRef.has_value())
      << "Failed to create reference: " << runnableRef.error().message();

    auto runtimeRef = package::Reference::parse("stable:org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(runtimeRef.has_value())
      << "Failed to create runtime reference: " << runtimeRef.error().message();

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    api::types::v1::RepositoryCacheLayersItem appItem;
    appItem.info.id = "org.example.runnable";
    appItem.info.version = "1.0.0";
    appItem.info.kind = "app";
    appItem.info.channel = "stable";
    appItem.info.arch = { std::string{ "x86_64" } };
    appItem.info.base = "org.deepin.base/23.0.0";
    appItem.info.runtime = "org.deepin.runtime/23.0.0";

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.id = "org.deepin.runtime";
    runtimeItem.info.version = "23.0.0";
    runtimeItem.info.kind = "runtime";
    runtimeItem.info.channel = "stable";
    runtimeItem.info.arch = { std::string{ "x86_64" } };
    runtimeItem.info.base = "org.deepin.base/23.0.0";

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*runnableRef, _, _)).WillOnce(Return(appItem));
    EXPECT_CALL(*repo, clearReference(_, _, _, _))
      .WillOnce(Return(*runtimeRef))
      .WillOnce(Return(*baseRef));

    EXPECT_CALL(*repo, getLayerItem(*runtimeRef, _, _)).WillOnce(Return(runtimeItem));
    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*runnableRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*runtimeRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(*runnableRef);
    ASSERT_TRUE(result.has_value()) << "Failed to resolve runnable: " << result.error().message();

    EXPECT_TRUE(context.getAppLayer().has_value());
    EXPECT_TRUE(context.getRuntimeLayer().has_value());
    EXPECT_TRUE(context.getBaseLayer().has_value());
    EXPECT_TRUE(context.getConfig().timezone.has_value());
    EXPECT_FALSE(context.getConfig().timezone->empty());
}

TEST_F(RunContextTest, resolveRunnableWithRuntime)
{
    LINGLONG_TRACE("resolveRunnableWithRuntime");

    auto runnableRef = package::Reference::parse("stable:org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(runnableRef.has_value())
      << "Failed to create reference: " << runnableRef.error().message();

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.id = "org.deepin.runtime";
    runtimeItem.info.version = "23.0.0";
    runtimeItem.info.kind = "runtime";
    runtimeItem.info.channel = "stable";
    runtimeItem.info.arch = { std::string{ "x86_64" } };
    runtimeItem.info.base = "org.deepin.base/23.0.0";

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*runnableRef, _, _)).WillOnce(Return(runtimeItem));
    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*baseRef));

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*runnableRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(*runnableRef);
    ASSERT_TRUE(result.has_value()) << "Failed to resolve runnable: " << result.error().message();

    EXPECT_FALSE(context.getAppLayer().has_value());
    EXPECT_TRUE(context.getRuntimeLayer().has_value());
    EXPECT_TRUE(context.getBaseLayer().has_value());
}

TEST_F(RunContextTest, resolveRunnableWithBase)
{
    LINGLONG_TRACE("resolveRunnableWithBase");

    auto runnableRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(runnableRef.has_value())
      << "Failed to create reference: " << runnableRef.error().message();

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*runnableRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*runnableRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(*runnableRef);
    ASSERT_TRUE(result.has_value()) << "Failed to resolve runnable: " << result.error().message();

    EXPECT_FALSE(context.getAppLayer().has_value());
    EXPECT_FALSE(context.getRuntimeLayer().has_value());
    EXPECT_TRUE(context.getBaseLayer().has_value());
}

TEST_F(RunContextTest, resolveRunnableWithInvalidKind)
{
    LINGLONG_TRACE("resolveRunnableWithInvalidKind");

    auto runnableRef = package::Reference::parse("stable:org.example.invalid/1.0.0/x86_64");
    ASSERT_TRUE(runnableRef.has_value())
      << "Failed to create reference: " << runnableRef.error().message();

    api::types::v1::RepositoryCacheLayersItem invalidItem;
    invalidItem.info.id = "org.example.invalid";
    invalidItem.info.version = "1.0.0";
    invalidItem.info.kind = "invalid";
    invalidItem.info.channel = "stable";
    invalidItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*runnableRef, _, _)).WillOnce(Return(invalidItem));

    RunContext context(*this->repo);
    auto result = context.resolve(*runnableRef);
    EXPECT_FALSE(result.has_value()) << "Expected resolve to fail for invalid kind";
    EXPECT_TRUE(result.error().message().find("kind") != std::string::npos
                || result.error().message().find("not runnable") != std::string::npos);
}

TEST_F(RunContextTest, toConfig)
{
    LINGLONG_TRACE("toConfig");

    auto runnableRef = package::Reference::parse("stable:org.example.runnable/1.0.0/x86_64");
    ASSERT_TRUE(runnableRef.has_value())
      << "Failed to create reference: " << runnableRef.error().message();

    auto runtimeRef = package::Reference::parse("stable:org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(runtimeRef.has_value())
      << "Failed to create runtime reference: " << runtimeRef.error().message();

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    auto extensionRef = package::Reference::parse("stable:org.example.extension/1.0.0/x86_64");
    ASSERT_TRUE(extensionRef.has_value())
      << "Failed to create extension reference: " << extensionRef.error().message();

    api::types::v1::RepositoryCacheLayersItem appItem;
    appItem.info.id = "org.example.runnable";
    appItem.info.version = "1.0.0";
    appItem.info.kind = "app";
    appItem.info.channel = "stable";
    appItem.info.arch = { std::string{ "x86_64" } };
    appItem.info.base = "org.deepin.base/23.0.0";
    appItem.info.runtime = "org.deepin.runtime/23.0.0";

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.id = "org.deepin.runtime";
    runtimeItem.info.version = "23.0.0";
    runtimeItem.info.kind = "runtime";
    runtimeItem.info.channel = "stable";
    runtimeItem.info.arch = { std::string{ "x86_64" } };
    runtimeItem.info.base = "org.deepin.base/23.0.0";

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    api::types::v1::RepositoryCacheLayersItem extensionItem;
    extensionItem.info.id = "org.example.extension";
    extensionItem.info.version = "1.0.0";
    extensionItem.info.kind = "extension";
    extensionItem.info.channel = "stable";
    extensionItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*runnableRef, _, _)).WillOnce(Return(appItem));
    EXPECT_CALL(*repo, clearReference(_, _, _, _))
      .Times(AtLeast(2))
      .WillOnce(Return(*runtimeRef))
      .WillOnce(Return(*baseRef))
      .WillOnce(Return(*extensionRef));

    EXPECT_CALL(*repo, getLayerItem(*runtimeRef, _, _)).WillOnce(Return(runtimeItem));
    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));
    EXPECT_CALL(*repo, getLayerItem(*extensionRef, _, _)).WillOnce(Return(extensionItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*runnableRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*runtimeRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*extensionRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(
      *runnableRef,
      ResolveOptions{ .extensionRefs = std::vector<std::string>{ extensionRef->toString() } });
    ASSERT_TRUE(result.has_value()) << "Failed to resolve runnable: " << result.error().message();

    ASSERT_TRUE(context.getBaseLayer().has_value());
    EXPECT_EQ(context.getBaseLayer()->getReference().toString(),
              "stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(context.getRuntimeLayer().has_value());
    EXPECT_EQ(context.getRuntimeLayer()->getReference().toString(),
              "stable:org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(context.getAppLayer().has_value());
    EXPECT_EQ(context.getAppLayer()->getReference().toString(),
              "stable:org.example.runnable/1.0.0/x86_64");
}

TEST_F(RunContextTest, resolveFromConfig)
{
    LINGLONG_TRACE("resolveFromConfig");

    auto runtimeRef = package::Reference::parse("stable:org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(runtimeRef.has_value())
      << "Failed to create runtime reference: " << runtimeRef.error().message();

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    auto appRef = package::Reference::parse("stable:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(appRef.has_value())
      << "Failed to create app reference: " << appRef.error().message();

    auto extensionRef = package::Reference::parse("stable:org.example.extension/1.0.0/x86_64");
    ASSERT_TRUE(extensionRef.has_value())
      << "Failed to create extension reference: " << extensionRef.error().message();

    api::types::v1::RunContextConfig config;
    config.version = "1";
    config.base = baseRef->toString();
    config.runtime = runtimeRef->toString();
    config.app = appRef->toString();
    config.extensions = std::map<std::string, std::vector<std::string>>{
        { appRef->toString(), std::vector<std::string>{ extensionRef->toString() } }
    };

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.id = "org.deepin.runtime";
    runtimeItem.info.version = "23.0.0";
    runtimeItem.info.kind = "runtime";
    runtimeItem.info.channel = "stable";
    runtimeItem.info.arch = { std::string{ "x86_64" } };

    api::types::v1::RepositoryCacheLayersItem appItem;
    appItem.info.id = "org.example.app";
    appItem.info.version = "1.0.0";
    appItem.info.kind = "app";
    appItem.info.channel = "stable";
    appItem.info.arch = { std::string{ "x86_64" } };

    api::types::v1::RepositoryCacheLayersItem extensionItem;
    extensionItem.info.id = "org.example.extension";
    extensionItem.info.version = "1.0.0";
    extensionItem.info.kind = "extension";
    extensionItem.info.channel = "stable";
    extensionItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));
    EXPECT_CALL(*repo, getLayerItem(*runtimeRef, _, _)).WillOnce(Return(runtimeItem));
    EXPECT_CALL(*repo, getLayerItem(*appRef, _, _)).WillOnce(Return(appItem));
    EXPECT_CALL(*repo, getLayerItem(*extensionRef, _, _)).WillOnce(Return(extensionItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*runtimeRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*appRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));
    EXPECT_CALL(*repo, getMergedModuleDir(*extensionRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(config);
    ASSERT_TRUE(result.has_value()) << "Failed to resolve config: " << result.error().message();

    auto &retConfig = context.getConfig();
    EXPECT_EQ(retConfig.version, "1");
    EXPECT_TRUE(retConfig.base.has_value());
    EXPECT_EQ(retConfig.base.value(), baseRef->toString());
    EXPECT_TRUE(retConfig.runtime.has_value());
    EXPECT_EQ(retConfig.runtime.value(), runtimeRef->toString());
    EXPECT_TRUE(retConfig.app.has_value());
    EXPECT_EQ(retConfig.app.value(), appRef->toString());
    ASSERT_TRUE(retConfig.extensions.has_value());
    ASSERT_FALSE(retConfig.extensions->empty());
    ASSERT_EQ(retConfig.extensions->size(), 1);

    EXPECT_TRUE(context.getBaseLayer().has_value());
    EXPECT_TRUE(context.getRuntimeLayer().has_value());
    EXPECT_TRUE(context.getAppLayer().has_value());
}

TEST_F(RunContextTest, resolveFromConfigBaseOnly)
{
    LINGLONG_TRACE("resolveFromConfigBaseOnly");

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    api::types::v1::RunContextConfig config;
    config.version = "1";
    config.base = baseRef->toString();
    config.runtime = std::nullopt;
    config.app = std::nullopt;
    config.extensions = std::nullopt;

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext context(*this->repo);
    auto result = context.resolve(config);
    ASSERT_TRUE(result.has_value()) << "Failed to resolve config: " << result.error().message();

    EXPECT_EQ(context.getConfig().version, "1");
    EXPECT_TRUE(context.getBaseLayer().has_value());
    EXPECT_FALSE(context.getRuntimeLayer().has_value());
    EXPECT_FALSE(context.getAppLayer().has_value());
}

TEST_F(RunContextTest, resolveWithCDIDevicesInOptions)
{
    LINGLONG_TRACE("resolveWithCDIDevicesInOptions");

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    const auto specPath = tempDir->path() / "vendor.yaml";
    std::ofstream spec(specPath);
    spec << R"(cdiVersion: "0.6.0"
kind: "vendor.com/device"
devices:
  - name: "gpu0"
    containerEdits:
      env:
        - "FOO=BAR"
)";
    spec.close();

    RunContext context(*this->repo);
    auto result = context.resolve(
      *baseRef,
      ResolveOptions{
        .cdiDevices = std::vector<api::types::v1::CdiDeviceEntry>{ api::types::v1::CdiDeviceEntry{
          .kind = "vendor.com/device",
          .name = "gpu0",
          .spec =
            api::types::v1::CdiSpec{
              .checksum = specChecksum(specPath),
              .path = specPath.string(),
            },
        } },
      });
    ASSERT_TRUE(result.has_value()) << "Failed to resolve config: " << result.error().message();

    const auto &retConfig = context.getConfig();
    ASSERT_TRUE(retConfig.cdiDevices.has_value());
    ASSERT_EQ(retConfig.cdiDevices->size(), 1);
    EXPECT_EQ(retConfig.cdiDevices->at(0).kind, "vendor.com/device");
    EXPECT_EQ(retConfig.cdiDevices->at(0).name, "gpu0");
    EXPECT_EQ(retConfig.cdiDevices->at(0).spec.path, specPath.string());
    EXPECT_FALSE(retConfig.cdiDevices->at(0).spec.checksum.empty());

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    RunContext restored(*this->repo);
    result = restored.resolve(retConfig);
    ASSERT_TRUE(result.has_value())
      << "Failed to resolve config with CDI: " << result.error().message();

    ASSERT_TRUE(restored.getConfig().cdiDevices.has_value());
    ASSERT_EQ(restored.getConfig().cdiDevices->size(), 1);
    EXPECT_EQ(restored.getConfig().cdiDevices->at(0).spec.path, specPath.string());
}

TEST_F(RunContextTest, cdiEnvPreservesEqualsInValue)
{
    LINGLONG_TRACE("cdiEnvPreservesEqualsInValue");

    auto baseRef = package::Reference::parse("stable:org.deepin.base/23.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value())
      << "Failed to create base reference: " << baseRef.error().message();

    api::types::v1::RepositoryCacheLayersItem baseItem;
    baseItem.info.id = "org.deepin.base";
    baseItem.info.version = "23.0.0";
    baseItem.info.kind = "base";
    baseItem.info.channel = "stable";
    baseItem.info.arch = { std::string{ "x86_64" } };

    EXPECT_CALL(*repo, getLayerItem(*baseRef, _, _)).WillOnce(Return(baseItem));

    package::LayerDir mockLayerDir(tempDir->path() / "merged");
    EXPECT_CALL(*repo, getMergedModuleDir(*baseRef, _, _))
      .WillOnce(Return(utils::error::Result<package::LayerDir>(mockLayerDir)));

    const auto specPath = tempDir->path() / "vendor-env.yaml";
    std::ofstream spec(specPath);
    spec << R"(cdiVersion: "0.6.0"
kind: "vendor.com/device"
devices:
  - name: "gpu0"
    containerEdits:
      env:
        - "FOO=a=b"
)";
    spec.close();

    RunContext context(*this->repo);
    auto result = context.resolve(
      *baseRef,
      ResolveOptions{
        .cdiDevices = std::vector<api::types::v1::CdiDeviceEntry>{ api::types::v1::CdiDeviceEntry{
          .kind = "vendor.com/device",
          .name = "gpu0",
          .spec =
            api::types::v1::CdiSpec{
              .checksum = specChecksum(specPath),
              .path = specPath.string(),
            },
        } },
      });
    ASSERT_TRUE(result.has_value()) << "Failed to resolve config: " << result.error().message();

    const auto basePath = tempDir->path() / "base";
    const auto bundlePath = tempDir->path() / "bundle";
    std::filesystem::create_directories(basePath);
    std::filesystem::create_directories(bundlePath);

    generator::ContainerCfgBuilder builder;
    builder.setAppId("org.deepin.base")
      .setBasePath(basePath)
      .setBundlePath(bundlePath)
      .disablePatch();

    result = context.setupCDIDevices(builder);
    ASSERT_TRUE(result.has_value()) << "Failed to apply CDI edits: " << result.error().message();

    result = builder.build();
    ASSERT_TRUE(result.has_value()) << "Failed to build OCI config: " << result.error().message();

    ASSERT_TRUE(builder.getConfig().process.has_value());
    ASSERT_TRUE(builder.getConfig().process->env.has_value());
    EXPECT_THAT(*builder.getConfig().process->env, ::testing::Contains("FOO=a=b"));
}

TEST_F(RunContextTest, resolveFromConfigVersionMismatch)
{
    LINGLONG_TRACE("resolveFromConfigVersionMismatch");

    api::types::v1::RunContextConfig config;
    config.version = "2";
    config.base = "stable:org.deepin.base/23.0.0/x86_64";

    RunContext context(*this->repo);
    auto result = context.resolve(config);
    ASSERT_FALSE(result.has_value()) << "Expected resolve to fail for version mismatch";
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("version mismatch"));
}

} // namespace
