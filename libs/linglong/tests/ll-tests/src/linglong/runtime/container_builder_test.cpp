// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/cli/cli.h"
#include "linglong/common/dir.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/env.h"

#include <filesystem>
#include <fstream>

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
    ASSERT_TRUE(tempDir.isValid());

    linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", tempDir.path().string());

    auto bundle = linglong::runtime::makeBundleDir("test-container");
    ASSERT_TRUE(bundle.has_value()) << bundle.error().message();

    EXPECT_EQ(*bundle, tempDir.path() / "linglong" / "test-container");
    EXPECT_TRUE(std::filesystem::is_directory(*bundle));
}

TEST(ContainerBuilderUtil, MakeBundleDirAppendsSuffix)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", tempDir.path().string());

    auto bundle = linglong::runtime::makeBundleDir("test-container", "-build");
    ASSERT_TRUE(bundle.has_value()) << bundle.error().message();

    EXPECT_EQ(*bundle, tempDir.path() / "linglong" / "test-container-build");
}

TEST(ContainerBuilderUtil, MakeBundleDirRemovesExistingContent)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

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
