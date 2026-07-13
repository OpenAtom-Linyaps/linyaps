// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/env.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/runtime_config.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace linglong::api::types::v1;
using namespace linglong::utils;

using linglong::utils::EnvironmentVariableGuard;

TEST(RuntimeConfigTest, LoadFromPath)
{
    TempDir tempDir;
    // Create a test config file
    fs::path config_file = tempDir.path() / "test_config.json";

    RuntimeConfigure config;
    config.disableXdp = true;
    config.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/bin" }, { "HOME", "/home/user" } };

    ExtensionDefine ext1;
    ext1.name = "test-extension";
    ext1.version = "1.0.0";
    ext1.directory = "/opt/extension";
    ext1.allowEnv = std::map<std::string, std::string>{ { "DISPLAY", ":0" } };

    config.extDefs =
      std::map<std::string, std::vector<ExtensionDefine>>{ { "test-app", { ext1 } } };

    // Write config to file
    std::ofstream file(config_file);
    nlohmann::json j;
    linglong::api::types::v1::to_json(j, config);
    file << j.dump();
    file.close();

    // Load config from file
    auto loaded_config = linglong::utils::loadRuntimeConfig(config_file);
    ASSERT_TRUE(loaded_config.has_value());

    ASSERT_TRUE(loaded_config->disableXdp.has_value());
    EXPECT_TRUE(*loaded_config->disableXdp);

    ASSERT_TRUE(loaded_config->deviceMode);
    EXPECT_EQ(loaded_config->deviceMode->size(), 1);
    EXPECT_EQ(loaded_config->deviceMode->at(0), DeviceOption::Passthru);

    ASSERT_TRUE(loaded_config->env);
    EXPECT_EQ(loaded_config->env->size(), 2);
    EXPECT_EQ(loaded_config->env->at("PATH"), "/usr/bin");
    EXPECT_EQ(loaded_config->env->at("HOME"), "/home/user");

    ASSERT_TRUE(loaded_config->extDefs);
    EXPECT_EQ(loaded_config->extDefs->size(), 1);
    EXPECT_EQ(loaded_config->extDefs->at("test-app").size(), 1);
    EXPECT_EQ(loaded_config->extDefs->at("test-app")[0].name, "test-extension");
}

TEST(RuntimeConfigTest, LoadFromPathNotExist)
{
    TempDir tempDir;
    fs::path non_existent_file = tempDir.path() / "non_existent.json";
    auto result = linglong::utils::loadRuntimeConfig(non_existent_file);
    EXPECT_FALSE(result);
}

TEST(RuntimeConfigTest, MergeConfigs)
{
    // Create first config
    RuntimeConfigure config1;
    config1.disableXdp = false;
    config1.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config1.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/bin" }, { "HOME", "/home/user1" } };

    ExtensionDefine ext1;
    ext1.name = "extension1";
    ext1.version = "1.0.0";
    ext1.directory = "/opt/ext1";
    config1.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{ { "app1", { ext1 } } };

    // Create second config
    RuntimeConfigure config2;
    config2.disableXdp = true;
    config2.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config2.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/local/bin" }, { "USER", "testuser" } };

    ExtensionDefine ext2;
    ext2.name = "extension2";
    ext2.version = "2.0.0";
    ext2.directory = "/opt/ext2";
    config2.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{
        { "app1", { ext2 } }, // Same app key as config1
        { "app2", { ext2 } }  // Different app key
    };

    // Merge configs
    std::vector<RuntimeConfigure> configs = { config1, config2 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    ASSERT_TRUE(merged.disableXdp.has_value());
    EXPECT_TRUE(*merged.disableXdp);

    ASSERT_TRUE(merged.deviceMode);
    EXPECT_EQ(merged.deviceMode->size(), 2);
    EXPECT_EQ(merged.deviceMode->at(0), DeviceOption::Passthru);
    EXPECT_EQ(merged.deviceMode->at(1), DeviceOption::Passthru);

    // Check environment variables
    ASSERT_TRUE(merged.env);
    EXPECT_EQ(merged.env->size(), 3);                    // PATH, HOME, USER
    EXPECT_EQ(merged.env->at("PATH"), "/usr/local/bin"); // config2 overrides config1
    EXPECT_EQ(merged.env->at("HOME"), "/home/user1");
    EXPECT_EQ(merged.env->at("USER"), "testuser");

    // Check extension definitions
    ASSERT_TRUE(merged.extDefs);
    EXPECT_EQ(merged.extDefs->size(), 2); // app1, app2

    // app1 should have both extensions
    EXPECT_EQ(merged.extDefs->at("app1").size(), 2);
    EXPECT_EQ(merged.extDefs->at("app1")[0].name, "extension1");
    EXPECT_EQ(merged.extDefs->at("app1")[1].name, "extension2");

    // app2 should have only extension2
    EXPECT_EQ(merged.extDefs->at("app2").size(), 1);
    EXPECT_EQ(merged.extDefs->at("app2")[0].name, "extension2");
}

TEST(RuntimeConfigTest, MergeEmptyConfigs)
{
    std::vector<RuntimeConfigure> empty_configs;
    auto merged = linglong::utils::MergeRuntimeConfig(empty_configs);

    EXPECT_FALSE(merged.disableXdp);
    EXPECT_FALSE(merged.deviceMode);
    EXPECT_FALSE(merged.env);
    EXPECT_FALSE(merged.extDefs);
}

TEST(RuntimeConfigTest, MergePartialConfigs)
{
    // Config with only env
    RuntimeConfigure config1;
    config1.disableXdp = false;
    config1.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config1.env = std::map<std::string, std::string>{ { "PATH", "/usr/bin" } };

    // Config empty
    RuntimeConfigure config2;

    // Config with only extDefs
    RuntimeConfigure config3;
    ExtensionDefine ext;
    ext.name = "test-ext";
    ext.version = "1.0.0";
    ext.directory = "/opt/ext";
    config3.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{ { "app", { ext } } };

    std::vector<RuntimeConfigure> configs = { config1, config2, config3 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    nlohmann::json j;
    linglong::api::types::v1::to_json(j, merged);
    LogD("{}", j.dump());

    ASSERT_TRUE(merged.disableXdp.has_value());
    EXPECT_FALSE(*merged.disableXdp);

    ASSERT_TRUE(merged.env);
    EXPECT_EQ(merged.env->size(), 1);
    EXPECT_EQ(merged.env->at("PATH"), "/usr/bin");

    ASSERT_TRUE(merged.deviceMode);
    EXPECT_EQ(merged.deviceMode->size(), 1);
    EXPECT_EQ(merged.deviceMode->at(0), DeviceOption::Passthru);

    ASSERT_TRUE(merged.extDefs);
    EXPECT_EQ(merged.extDefs->size(), 1);
    EXPECT_EQ(merged.extDefs->at("app").size(), 1);
    EXPECT_EQ(merged.extDefs->at("app")[0].name, "test-ext");
}

TEST(RuntimeConfigTest, MergeMounts)
{
    RuntimeConfigure config1;
    config1.mounts = std::vector<Mount>{
        { .destination = "/tmp/a", .source = "/host/a", .type = "bind" },
    };

    RuntimeConfigure config2;
    config2.mounts = std::vector<Mount>{
        { .destination = "/tmp/b", .source = "/host/b", .type = "bind" },
    };

    std::vector<RuntimeConfigure> configs = { config1, config2 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    ASSERT_TRUE(merged.mounts.has_value());
    EXPECT_EQ(merged.mounts->size(), 2);
    EXPECT_EQ(merged.mounts->at(0).destination, "/tmp/a");
    EXPECT_EQ(merged.mounts->at(1).destination, "/tmp/b");
}

TEST(RuntimeConfigTest, MergeInstances)
{
    RuntimeConfigure config1;
    config1.disableXdp = false;
    config1.instances = std::map<std::string, RuntimeConfigure>{
        { "dev",
          RuntimeConfigure{ .disableXdp = true,
                            .env = std::map<std::string, std::string>{ { "DEBUG", "1" } } } },
        { "prod", RuntimeConfigure{ .disableXdp = false } },
    };

    RuntimeConfigure config2;
    config2.instances = std::map<std::string, RuntimeConfigure>{
        { "dev",
          RuntimeConfigure{ .env = std::map<std::string, std::string>{ { "VERBOSE", "1" } } } },
        { "test", RuntimeConfigure{ .disableXdp = true } },
    };

    std::vector<RuntimeConfigure> configs = { config1, config2 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    ASSERT_TRUE(merged.instances.has_value());
    EXPECT_EQ(merged.instances->size(), 3);

    // dev should be merged recursively
    auto &devInstance = (*merged.instances)["dev"];
    EXPECT_TRUE(devInstance.disableXdp.has_value());
    EXPECT_TRUE(*devInstance.disableXdp);
    ASSERT_TRUE(devInstance.env.has_value());
    EXPECT_EQ(devInstance.env->size(), 2);
    EXPECT_EQ(devInstance.env->at("DEBUG"), "1");
    EXPECT_EQ(devInstance.env->at("VERBOSE"), "1");

    // prod should be from config1
    EXPECT_TRUE((*merged.instances)["prod"].disableXdp.has_value());
    EXPECT_FALSE(*(*merged.instances)["prod"].disableXdp);

    // test should be from config2
    EXPECT_TRUE((*merged.instances)["test"].disableXdp.has_value());
    EXPECT_TRUE(*(*merged.instances)["test"].disableXdp);
}

TEST(RuntimeConfigTest, MergeInstancesWithMounts)
{
    RuntimeConfigure config1;
    config1.mounts = std::vector<Mount>{
        { .destination = "/tmp/a", .source = "/host/a", .type = "bind" },
    };
    config1.instances = std::map<std::string, RuntimeConfigure>{
        { "dev",
          RuntimeConfigure{
            .mounts =
              std::vector<Mount>{
                { .destination = "/tmp/instance-a", .source = "/host/instance-a", .type = "bind" },
              } } },
    };

    RuntimeConfigure config2;
    config2.mounts = std::vector<Mount>{
        { .destination = "/tmp/b", .source = "/host/b", .type = "bind" },
    };
    config2.instances = std::map<std::string, RuntimeConfigure>{
        { "dev",
          RuntimeConfigure{
            .mounts =
              std::vector<Mount>{
                { .destination = "/tmp/instance-b", .source = "/host/instance-b", .type = "bind" },
              } } },
    };

    std::vector<RuntimeConfigure> configs = { config1, config2 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    // Base mounts should be merged
    ASSERT_TRUE(merged.mounts.has_value());
    EXPECT_EQ(merged.mounts->size(), 2);
    EXPECT_EQ(merged.mounts->at(0).destination, "/tmp/a");
    EXPECT_EQ(merged.mounts->at(1).destination, "/tmp/b");

    // dev instance mounts should be merged
    ASSERT_TRUE(merged.instances.has_value());
    auto &devInstance = (*merged.instances)["dev"];
    ASSERT_TRUE(devInstance.mounts.has_value());
    EXPECT_EQ(devInstance.mounts->size(), 2);
    EXPECT_EQ(devInstance.mounts->at(0).destination, "/tmp/instance-a");
    EXPECT_EQ(devInstance.mounts->at(1).destination, "/tmp/instance-b");
}

TEST(RuntimeConfigTest, LoadRuntimeConfigWithInstanceMounts)
{
    TempDir tempDir;

    RuntimeConfigure config;
    config.disableXdp = false;
    config.mounts = std::vector<Mount>{
        { .destination = "/tmp/base", .source = "/host/base", .type = "bind" },
    };
    config.instances = std::map<std::string, RuntimeConfigure>{
        { "dev",
          RuntimeConfigure{
            .disableXdp = true,
            .env = std::map<std::string, std::string>{ { "DEBUG", "1" } },
            .mounts =
              std::vector<Mount>{
                { .destination = "/tmp/dev", .source = "/host/dev", .type = "bind" },
              },
          } },
    };

    fs::path configDir = tempDir.path() / "linglong" / "apps" / "test-app";
    fs::create_directories(configDir);

    fs::path configFile = configDir / "config.json";
    std::ofstream file(configFile);
    nlohmann::json j;
    linglong::api::types::v1::to_json(j, config);
    file << j.dump();
    file.close();

    std::vector<std::filesystem::path> configDirs = { tempDir.path() / "linglong" };

    auto loadedDefault = linglong::utils::loadRuntimeConfig(configDirs, "test-app", "");
    ASSERT_TRUE(loadedDefault.has_value());
    EXPECT_TRUE(loadedDefault->has_value());
    auto &defaultConfig = **loadedDefault;
    EXPECT_FALSE(*defaultConfig.disableXdp);
    ASSERT_TRUE(defaultConfig.mounts.has_value());
    EXPECT_EQ(defaultConfig.mounts->size(), 1);
    EXPECT_EQ(defaultConfig.mounts->at(0).destination, "/tmp/base");
    EXPECT_FALSE(defaultConfig.instances.has_value());

    auto loadedDev = linglong::utils::loadRuntimeConfig(configDirs, "test-app", "dev");
    ASSERT_TRUE(loadedDev.has_value());
    EXPECT_TRUE(loadedDev->has_value());
    auto &devConfig = **loadedDev;
    EXPECT_TRUE(*devConfig.disableXdp);
    ASSERT_TRUE(devConfig.mounts.has_value());
    EXPECT_EQ(devConfig.mounts->size(), 2);
    ASSERT_TRUE(devConfig.env.has_value());
    EXPECT_EQ(devConfig.env->at("DEBUG"), "1");
    EXPECT_FALSE(devConfig.instances.has_value());
}
