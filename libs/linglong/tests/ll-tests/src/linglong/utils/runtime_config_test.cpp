// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/log/log.h"
#include "linglong/utils/runtime_config.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace linglong::api::types::v1;
using namespace linglong::utils;

TEST(RuntimeConfigTest, LoadFromPath)
{
    TempDir tempDir;
    // Create a test config file
    fs::path config_file = tempDir.path() / "test_config.json";

    RuntimeConfigure config;
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
    config1.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/bin" }, { "HOME", "/home/user1" } };

    ExtensionDefine ext1;
    ext1.name = "extension1";
    ext1.version = "1.0.0";
    ext1.directory = "/opt/ext1";
    config1.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{ { "app1", { ext1 } } };

    // Create second config
    RuntimeConfigure config2;
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

    EXPECT_FALSE(merged.env);
    EXPECT_FALSE(merged.extDefs);
}

TEST(RuntimeConfigTest, MergePartialConfigs)
{
    // Config with only env
    RuntimeConfigure config1;
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

    ASSERT_TRUE(merged.env);
    EXPECT_EQ(merged.env->size(), 1);
    EXPECT_EQ(merged.env->at("PATH"), "/usr/bin");

    ASSERT_TRUE(merged.extDefs);
    EXPECT_EQ(merged.extDefs->size(), 1);
    EXPECT_EQ(merged.extDefs->at("app").size(), 1);
    EXPECT_EQ(merged.extDefs->at("app")[0].name, "test-ext");
}
