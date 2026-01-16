/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/builder/config.h"
#include "linglong/utils/env.h"

#include <filesystem>
#include <fstream>
#include <optional>

using namespace linglong::builder;
using namespace linglong::api::types::v1;
using linglong::utils::EnvironmentVariableGuard;

class BuilderConfigTest : public ::testing::Test
{
protected:
    void SetUp() override { tempDir = std::make_unique<TempDir>("ll_builder_test_"); }

    std::filesystem::path testDir() const { return tempDir->path(); }

private:
    std::unique_ptr<TempDir> tempDir;
};

TEST_F(BuilderConfigTest, SaveConfig)
{
    BuilderConfig config;
    config.version = 1;
    config.repo = "/test/repo";
    config.arch = "x86_64";
    config.cache = "/test/cache";
    config.offline = true;

    auto configPath = testDir() / "test_config.yaml";
    auto result = saveConfig(config, configPath);

    ASSERT_TRUE(result.has_value()) << "Failed to save config: " << result.error().message();
    ASSERT_TRUE(std::filesystem::exists(configPath));
}

TEST_F(BuilderConfigTest, SaveConfigInvalidPath)
{
    BuilderConfig config;
    config.version = 1;
    config.repo = "/test/repo";

    // Use an invalid path (directory that doesn't exist and can't be created)
    auto invalidPath = testDir() / "nonexistent/config.yaml";
    auto result = saveConfig(config, invalidPath);

    ASSERT_FALSE(result.has_value());
}

TEST_F(BuilderConfigTest, LoadConfigFromPath)
{
    // First create a config file
    BuilderConfig originalConfig;
    originalConfig.version = 1;
    originalConfig.repo = "/test/repo";
    originalConfig.arch = "x86_64";
    originalConfig.cache = "/test/cache";
    originalConfig.offline = false;

    auto configPath = testDir() / "test_config.yaml";
    auto saveResult = saveConfig(originalConfig, configPath);
    ASSERT_TRUE(saveResult.has_value());

    // Now load it
    auto loadResult = loadConfig(configPath);
    ASSERT_TRUE(loadResult.has_value())
      << "Failed to load config: " << loadResult.error().message();
    EXPECT_EQ(loadResult->version, 1);
    EXPECT_EQ(loadResult->repo, "/test/repo");
    EXPECT_EQ(loadResult->arch, "x86_64");
    EXPECT_EQ(loadResult->cache, "/test/cache");
    EXPECT_EQ(loadResult->offline, false);
}

TEST_F(BuilderConfigTest, LoadConfigFromPathInvalidVersion)
{
    // Create a config file with wrong version
    BuilderConfig config;
    config.version = 2; // Wrong version
    config.repo = "/test/repo";

    auto configPath = testDir() / "invalid_version_config.yaml";
    auto saveResult = saveConfig(config, configPath);
    ASSERT_TRUE(saveResult.has_value());

    // Try to load it - should fail
    auto loadResult = loadConfig(configPath);
    ASSERT_FALSE(loadResult.has_value());
    EXPECT_TRUE(loadResult.error().message().find("wrong configuration file version")
                != std::string::npos);
}

TEST_F(BuilderConfigTest, LoadConfigFromPathNonExistent)
{
    auto nonExistentPath = testDir() / "nonexistent.yaml";
    auto result = loadConfig(nonExistentPath);
    ASSERT_FALSE(result.has_value());
}

TEST_F(BuilderConfigTest, InitDefaultBuildConfig)
{
    EnvironmentVariableGuard homeGuard("HOME", testDir().string());
    EnvironmentVariableGuard xdgCacheGuard("XDG_CACHE_HOME", (testDir() / "cache").string());

    auto configPath = testDir() / "builder" / "config.yaml";
    auto result = initDefaultBuildConfig(configPath);

    ASSERT_TRUE(result.has_value())
      << "Failed to init default config: " << result.error().message();

    EXPECT_EQ(result->version, 1);
    EXPECT_TRUE(result->repo.find("linglong-builder") != std::string::npos);
    EXPECT_TRUE(std::filesystem::exists(configPath));
}

TEST_F(BuilderConfigTest, InitDefaultBuildConfigNoCacheDir)
{
    EnvironmentVariableGuard homeGuard("HOME", "");
    EnvironmentVariableGuard xdgCacheGuard("XDG_CACHE_HOME", "");

    auto configPath = testDir() / "builder" / "config.yaml";
    auto result = initDefaultBuildConfig(configPath);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message().find("failed to get cache dir") != std::string::npos);
}

TEST_F(BuilderConfigTest, InitDefaultBuildConfigCreateDirFailure)
{
    EnvironmentVariableGuard homeGuard("HOME", testDir().string());
    EnvironmentVariableGuard xdgCacheGuard("XDG_CACHE_HOME", (testDir() / "cache").string());

    // Use a path where parent directory creation would fail
    auto invalidConfigPath = "config.yaml";
    auto result = initDefaultBuildConfig(invalidConfigPath);

    ASSERT_FALSE(result.has_value());
}

TEST_F(BuilderConfigTest, LoadConfigDefaultPath)
{
    EnvironmentVariableGuard homeGuard("HOME", testDir().string());
    EnvironmentVariableGuard xdgRuntimeGuard("XDG_CONFIG_HOME", (testDir() / "config").string());
    EnvironmentVariableGuard xdgCacheGuard("XDG_CACHE_HOME", (testDir() / "cache").string());

    // This should create a default config since none exists
    auto result = loadConfig();
    ASSERT_TRUE(result.has_value())
      << "Failed to load default config: " << result.error().message();

    EXPECT_EQ(result->version, 1);
    EXPECT_EQ(result->repo, testDir() / "cache/linglong-builder");
}

TEST_F(BuilderConfigTest, LoadConfigDefaultPathMissingConfigDir)
{
    EnvironmentVariableGuard homeGuard("HOME", "");
    EnvironmentVariableGuard xdgRuntimeGuard("XDG_CONFIG_HOME", "");

    auto result = loadConfig();
    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(result.error().message().find("failed to get config dir") != std::string::npos);
}

TEST_F(BuilderConfigTest, ConfigWithOptionalFields)
{
    // Test config with minimal required fields only
    BuilderConfig minimalConfig;
    minimalConfig.version = 1;
    minimalConfig.repo = "/minimal/repo";
    // arch, cache, offline are optional and left unset

    auto configPath = testDir() / "minimal_config.yaml";

    // Save and load minimal config
    auto saveResult = saveConfig(minimalConfig, configPath);
    ASSERT_TRUE(saveResult.has_value());

    auto loadResult = loadConfig(configPath);
    ASSERT_TRUE(loadResult.has_value());

    EXPECT_EQ(loadResult->version, 1);
    EXPECT_EQ(loadResult->repo, "/minimal/repo");
    EXPECT_FALSE(loadResult->arch.has_value());
    EXPECT_FALSE(loadResult->cache.has_value());
    EXPECT_FALSE(loadResult->offline.has_value());
}
