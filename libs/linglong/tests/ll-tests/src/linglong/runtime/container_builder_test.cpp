// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/cli.h"
#include "linglong/common/dir.h"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/env.h"
#include "ocppi/cli/crun/Crun.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/runtime/config/types/Process.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace linglong;

TEST(RunContainerOptionsTest, ApplyRuntimeConfigEnablesDevicePassthru)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.deviceMode =
      std::vector<api::types::v1::DeviceOption>{ api::types::v1::DeviceOption::Passthru };

    auto result = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}

TEST(RunContainerOptionsTest, ApplyRuntimeConfigDisablesXdp)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.disableXdp = true;

    auto result = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isXdpDisabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsEnablesDevicePassthru)
{
    runtime::RunContainerOptions options;
    cli::RunOptions runOptions;
    runOptions.deviceOptions = { api::types::v1::DeviceOption::Passthru };

    auto result = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsPreservesRuntimeConfigXdpSettingByDefault)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.disableXdp = true;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_TRUE(options.isXdpDisabled());

    cli::RunOptions runOptions;
    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isXdpDisabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsOverridesRuntimeConfigXdpSettingWhenSpecified)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.disableXdp = false;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_FALSE(options.isXdpDisabled());

    cli::RunOptions runOptions;
    runOptions.disableXdp = true;

    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isXdpDisabled());
}

TEST(RunContainerOptionsTest, ApplyRuntimeConfigEnablesPipewireSocketMount)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enablePipewire = true;

    auto result = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isPipewireSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsPreservesRuntimeConfigPipewireSettingByDefault)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enablePipewire = true;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_TRUE(options.isPipewireSocketMountEnabled());

    cli::RunOptions runOptions;
    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isPipewireSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsOverridesRuntimeConfigPipewireSettingWhenSpecified)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enablePipewire = false;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_FALSE(options.isPipewireSocketMountEnabled());

    cli::RunOptions runOptions;
    runOptions.enablePipewireSocketMount = true;

    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isPipewireSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyRuntimeConfigEnablesAtSpiSocketMount)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enableAtspi = true;

    auto result = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(result);
    EXPECT_TRUE(options.isAtSpiSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsPreservesRuntimeConfigAtSpiSettingByDefault)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enableAtspi = true;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_TRUE(options.isAtSpiSocketMountEnabled());

    cli::RunOptions runOptions;
    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isAtSpiSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsOverridesRuntimeConfigAtSpiSettingWhenSpecified)
{
    runtime::RunContainerOptions options;

    api::types::v1::RuntimeConfigure runtimeConfig;
    runtimeConfig.enableAtspi = false;

    auto runtimeResult = options.applyRuntimeConfig(runtimeConfig);
    ASSERT_TRUE(runtimeResult);
    ASSERT_FALSE(options.isAtSpiSocketMountEnabled());

    cli::RunOptions runOptions;
    runOptions.enableAtSpiSocketMount = true;

    auto cliResult = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(cliResult);
    EXPECT_TRUE(options.isAtSpiSocketMountEnabled());
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsRejectsMalformedEnv)
{
    runtime::RunContainerOptions options;
    cli::RunOptions runOptions;
    runOptions.envs = { "NOEQUALS" };
    auto result = options.applyCliRunOptions(runOptions);
    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("invalid environment variable"));
}

TEST(RunContainerOptionsTest, ApplyCliRunOptionsParsesEnvAndCaps)
{
    runtime::RunContainerOptions options;
    cli::RunOptions runOptions;
    runOptions.envs = { "FOO=bar", "EMPTY=" };
    runOptions.capsAdd = { "CAP_SYS_ADMIN" };
    runOptions.privileged = true;
    runOptions.deviceOptions = { api::types::v1::DeviceOption::Passthru };

    auto result = options.applyCliRunOptions(runOptions);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(options.getEnv().at("FOO"), "bar");
    EXPECT_EQ(options.getEnv().at("EMPTY"), "");
    EXPECT_THAT(options.getCapabilities(), ::testing::Contains("CAP_SYS_ADMIN"));
    EXPECT_TRUE(options.isPrivileged());
    EXPECT_TRUE(options.isDevicePassthruEnabled());
}

TEST(RunContainerOptionsTest, RootUidAutomaticallyUsesPrivilegedMode)
{
    runtime::RunContainerOptions options;

    EXPECT_TRUE(options.shouldUsePrivilegedMode(0));
}

TEST(RunContainerOptionsTest, NonRootUidDoesNotAutomaticallyUsePrivilegedMode)
{
    runtime::RunContainerOptions options;

    EXPECT_FALSE(options.shouldUsePrivilegedMode(1000));
}

TEST(RunContainerOptionsTest, ExplicitPrivilegedModeIsPreservedForNonRootUid)
{
    runtime::RunContainerOptions options;
    options.privileged = true;

    EXPECT_TRUE(options.shouldUsePrivilegedMode(1000));
}

TEST(RunContainerOptionsTest, EnableSecurityContext)
{
    runtime::RunContainerOptions options;
    options.enableSecurityContext({ runtime::SecurityContextType::WAYLAND });
    options.enableSecurityContext({ runtime::SecurityContextType::WAYLAND });
    EXPECT_EQ(options.getSecurityContexts().size(), 1);
    EXPECT_EQ(options.getSecurityContexts().front(), runtime::SecurityContextType::WAYLAND);
}

TEST(RunContainerOptionsTest, DefaultStateIsDisabled)
{
    runtime::RunContainerOptions options;
    EXPECT_FALSE(options.isXdpDisabled());
    EXPECT_FALSE(options.isPipewireSocketMountEnabled());
    EXPECT_FALSE(options.isAtSpiSocketMountEnabled());
    EXPECT_FALSE(options.isPrivileged());
    EXPECT_FALSE(options.isDevicePassthruEnabled());
    EXPECT_TRUE(options.getEnv().empty());
    EXPECT_TRUE(options.getCapabilities().empty());
    EXPECT_TRUE(options.getSecurityContexts().empty());
}

TEST(ContainerBuilderUtil, GenContainerIDIsDeterministic)
{
    api::types::v1::RunContextConfig cfg;
    cfg.version = "1";
    cfg.base = "stable:org.deepin.base/23/x86_64";

    EXPECT_EQ(runtime::genContainerID(cfg), runtime::genContainerID(cfg));

    api::types::v1::RunContextConfig different = cfg;
    different.base = "stable:org.deepin.base/24/x86_64";
    EXPECT_NE(runtime::genContainerID(cfg), runtime::genContainerID(different));
}

TEST(SecurityContextTest, FromAndToType)
{
    EXPECT_EQ(runtime::fromType(runtime::SecurityContextType::WAYLAND), "wayland");
    EXPECT_EQ(runtime::fromType(runtime::SecurityContextType::UNKNOWN), "unknown");
    EXPECT_EQ(runtime::fromType(static_cast<runtime::SecurityContextType>(99)), "unknown");

    EXPECT_EQ(runtime::toType("wayland"), runtime::SecurityContextType::WAYLAND);
    EXPECT_EQ(runtime::toType("bogus"), runtime::SecurityContextType::UNKNOWN);
}

TEST(SecurityContextTest, GetManagerReturnsNullForUnknownType)
{
    auto mgr = runtime::getSecurityContextManager(runtime::SecurityContextType::UNKNOWN);
    EXPECT_EQ(mgr, nullptr);
}

TEST(ContainerBuilderUtil, MakeBundleDirCreatesBundleDirectory)
{
    TempDir tempDir;
    ASSERT_TRUE(std::filesystem::is_directory(tempDir.path()));

    linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", tempDir.path().string());

    auto bundle = linglong::runtime::makeBundleDir("test-container");
    ASSERT_TRUE(bundle.has_value()) << bundle.error().message();

    EXPECT_EQ(*bundle, tempDir.path() / "linglong" / "test-container");
    EXPECT_TRUE(std::filesystem::is_directory(*bundle));
}

TEST(ContainerBuilderUtil, MakeBundleDirAppendsSuffix)
{
    TempDir tempDir;
    ASSERT_TRUE(std::filesystem::is_directory(tempDir.path()));

    linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", tempDir.path().string());

    auto bundle = linglong::runtime::makeBundleDir("test-container", "-build");
    ASSERT_TRUE(bundle.has_value()) << bundle.error().message();

    EXPECT_EQ(*bundle, tempDir.path() / "linglong" / "test-container-build");
}

TEST(ContainerBuilderUtil, MakeBundleDirRemovesExistingContent)
{
    TempDir tempDir;
    ASSERT_TRUE(std::filesystem::is_directory(tempDir.path()));

    linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", tempDir.path().string());

    auto first = linglong::runtime::makeBundleDir("test-container");
    ASSERT_TRUE(first.has_value()) << first.error().message();
    std::ofstream{ *first / "stale.txt" } << "stale content";
    ASSERT_TRUE(std::filesystem::exists(*first / "stale.txt"));

    auto bundle = linglong::runtime::makeBundleDir("test-container");
    ASSERT_TRUE(bundle.has_value()) << bundle.error().message();

    EXPECT_TRUE(std::filesystem::is_directory(*bundle));
    EXPECT_FALSE(std::filesystem::exists(*bundle / "stale.txt"));
}

TEST(SecurityContextTest, DefaultSecurityContextsAreRecognizedAndRoundTrip)
{
    auto &ctxs = runtime::getDefaultSecurityContexts();

    for (const auto ctx : ctxs) {
        EXPECT_NE(ctx, runtime::SecurityContextType::UNKNOWN) << "default context is unknown";
        EXPECT_EQ(runtime::toType(runtime::fromType(ctx)), ctx) << "context does not round trip";
    }
}

namespace {

class ScopedUnsetEnv
{
public:
    explicit ScopedUnsetEnv(const char *name)
        : name_(name)
    {
        const auto *value = ::getenv(name);
        if (value != nullptr) {
            value_ = value;
            existed_ = true;
        }
        ::unsetenv(name);
    }

    ~ScopedUnsetEnv()
    {
        if (existed_) {
            ::setenv(name_.c_str(), value_.c_str(), 1);
        }
    }

    ScopedUnsetEnv(const ScopedUnsetEnv &) = delete;
    ScopedUnsetEnv &operator=(const ScopedUnsetEnv &) = delete;

private:
    std::string name_;
    std::string value_;
    bool existed_{ false };
};

class FakeResolveRepo : public repo::OSTreeRepo
{
public:
    explicit FakeResolveRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &, std::string),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::LayerDir>,
                getMergedModuleDir,
                (const package::Reference &, bool),
                (override, const, noexcept));
};

class ContainerBuilderCreateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        xdgRuntimeEnv_ =
          std::make_unique<linglong::utils::EnvironmentVariableGuard>("XDG_RUNTIME_DIR",
                                                                      tempDir->path().string());
        homeEnv_ =
          std::make_unique<linglong::utils::EnvironmentVariableGuard>("HOME",
                                                                      tempDir->path().string());
        repo = std::make_unique<FakeResolveRepo>(tempDir->path() / "repo");
        cli = ocppi::cli::crun::Crun::New("/bin/false").value();
        builder = std::make_unique<runtime::ContainerBuilder>(*cli);

        std::filesystem::create_directories(tempDir->path() / ".ssh");
        std::filesystem::create_directories(tempDir->path() / ".gnupg");
    }

    void TearDown() override
    {
        builder.reset();
        cli.reset();
        repo.reset();
        homeEnv_.reset();
        xdgRuntimeEnv_.reset();
        tempDir.reset();
    }

    package::Reference addBaseLayer(const std::string &version)
    {
        auto ref = package::Reference::parse("stable:org.deepin.base/" + version + "/x86_64");
        EXPECT_TRUE(ref.has_value()) << ref.error().message();

        api::types::v1::RepositoryCacheLayersItem item;
        item.info.id = "org.deepin.base";
        item.info.version = version;
        item.info.kind = "base";
        item.info.channel = "stable";
        item.info.arch = { "x86_64" };
        item.info.packageInfoV2Module = "binary";
        item.commit = "base-commit";

        auto layerDir = tempDir->path() / ("base-" + version);
        std::filesystem::create_directories(layerDir / "files");

        EXPECT_CALL(*repo, getLayerItem(*ref, testing::_)).WillRepeatedly(testing::Invoke([item]() {
            return utils::error::Result<api::types::v1::RepositoryCacheLayersItem>{ item };
        }));
        EXPECT_CALL(*repo, getMergedModuleDir(*ref, true))
          .WillRepeatedly(testing::Invoke([layerDir]() {
              return utils::error::Result<package::LayerDir>{ package::LayerDir{ layerDir } };
          }));
        return *ref;
    }

    void resolveBase(runtime::RunContext &context, const package::Reference &ref)
    {
        api::types::v1::RunContextConfig config;
        config.version = "1";
        config.base = ref.toString();
        auto resolved = context.resolve(config);
        ASSERT_TRUE(resolved) << resolved.error().message();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<linglong::utils::EnvironmentVariableGuard> xdgRuntimeEnv_;
    std::unique_ptr<linglong::utils::EnvironmentVariableGuard> homeEnv_;
    std::unique_ptr<FakeResolveRepo> repo;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    std::unique_ptr<runtime::ContainerBuilder> builder;
};

} // namespace

TEST_F(ContainerBuilderCreateTest, CreateRunContainerBuildsBundleAndRunsRuntime)
{
    const auto baseRef = addBaseLayer("23.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    runtime::RunContainerOptions options;
    options.disableXdp = true;
    options.common.containerCachePath = tempDir->path() / "cache";
    std::filesystem::create_directories(*options.common.containerCachePath);

    auto container = builder->createRunContainer(context, options);
    ASSERT_TRUE(container) << container.error().message();

    const auto bundle = (*container)->bundle();
    EXPECT_TRUE(std::filesystem::is_directory(bundle));

    ocppi::runtime::config::types::Process process;
    process.args = std::vector<std::string>{ "echo", "hello" };
    auto run = (*container)->run(process, {});
    EXPECT_FALSE(run) << "the /bin/false runtime should report a failure";
    EXPECT_TRUE(std::filesystem::is_regular_file(bundle / "config.json"));
    EXPECT_TRUE(std::filesystem::is_regular_file(bundle / "entrypoint.sh"));
}

TEST_F(ContainerBuilderCreateTest, CreateInitContainerBuildsBundle)
{
    const auto baseRef = addBaseLayer("24.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    auto container = builder->createInitContainer(context);
    ASSERT_TRUE(container) << container.error().message();
    EXPECT_TRUE(std::filesystem::is_directory((*container)->bundle()));
}

TEST_F(ContainerBuilderCreateTest, CreateBuildContainerUsesExplicitBasePath)
{
    const auto baseRef = addBaseLayer("25.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    const auto basePath = tempDir->path() / "build-base";
    std::filesystem::create_directories(basePath / "files");
    runtime::BuilderContainerOptions options;
    options.basePath = basePath / "files";
    options.common.containerCachePath = tempDir->path() / "build-cache";
    std::filesystem::create_directories(*options.common.containerCachePath);

    auto container = builder->createBuildContainer(context, options);
    ASSERT_TRUE(container) << container.error().message();
    EXPECT_TRUE(std::filesystem::is_directory((*container)->bundle()));
}

TEST_F(ContainerBuilderCreateTest, CreateRunContainerAppliesEnvironmentCapabilitiesAndDevices)
{
    const auto baseRef = addBaseLayer("26.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    runtime::RunContainerOptions options;
    options.disableXdp = false;
    options.devicePassthru = true;
    options.enablePipewireSocketMount = true;
    options.enableAtSpiSocketMount = true;
    options.env = { { "LINGLONG_TEST_ENV", "value" } };
    options.capabilities = { "CAP_NET_RAW" };

    auto container = builder->createRunContainer(context, options);
    ASSERT_TRUE(container) << container.error().message();

    const auto envScript = (*container)->bundle() / "00env.sh";
    ASSERT_TRUE(std::filesystem::is_regular_file(envScript));
    std::ifstream input(envScript);
    const std::string content{ std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>() };
    EXPECT_THAT(content, testing::HasSubstr("LINGLONG_TEST_ENV='value'"));
}

TEST_F(ContainerBuilderCreateTest, CreateRunContainerWithoutHomeReportsError)
{
    const auto baseRef = addBaseLayer("27.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    ScopedUnsetEnv unsetHome("HOME");
    runtime::RunContainerOptions options;
    options.disableXdp = true;

    auto container = builder->createRunContainer(context, options);
    ASSERT_FALSE(container);
    EXPECT_THAT(container.error().message(), testing::HasSubstr("HOME is not set"));
}

TEST_F(ContainerBuilderCreateTest, CreateRunContainerHonorsPrivilegedModeByUid)
{
    const auto baseRef = addBaseLayer("28.0.0");
    runtime::RunContext context(*repo);
    resolveBase(context, baseRef);

    runtime::RunContainerOptions options;
    options.disableXdp = true;
    options.privileged = true;

    auto container = builder->createRunContainer(context, options);
    if (getuid() == 0) {
        EXPECT_TRUE(container) << container.error().message();
    } else {
        ASSERT_FALSE(container);
        EXPECT_THAT(container.error().message(),
                    testing::HasSubstr("privileged mode requires running as root"));
    }
}
