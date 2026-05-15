// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
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
    fs::path config_file = tempDir.path() / "test_config.json";

    RuntimeConfigure config;
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

    std::ofstream file(config_file);
    nlohmann::json j;
    linglong::api::types::v1::to_json(j, config);
    file << j.dump();
    file.close();

    auto loaded_config = linglong::utils::loadRuntimeConfig(config_file);
    ASSERT_TRUE(loaded_config.has_value());

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
    RuntimeConfigure config1;
    config1.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config1.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/bin" }, { "HOME", "/home/user1" } };

    ExtensionDefine ext1;
    ext1.name = "extension1";
    ext1.version = "1.0.0";
    ext1.directory = "/opt/ext1";
    config1.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{ { "app1", { ext1 } } };

    RuntimeConfigure config2;
    config2.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config2.env =
      std::map<std::string, std::string>{ { "PATH", "/usr/local/bin" }, { "USER", "testuser" } };

    ExtensionDefine ext2;
    ext2.name = "extension2";
    ext2.version = "2.0.0";
    ext2.directory = "/opt/ext2";
    config2.extDefs = std::map<std::string, std::vector<ExtensionDefine>>{ { "app1", { ext2 } },
                                                                           { "app2", { ext2 } } };

    std::vector<RuntimeConfigure> configs = { config1, config2 };
    auto merged = linglong::utils::MergeRuntimeConfig(configs);

    ASSERT_TRUE(merged.deviceMode);
    EXPECT_EQ(merged.deviceMode->size(), 2);
    EXPECT_EQ(merged.deviceMode->at(0), DeviceOption::Passthru);
    EXPECT_EQ(merged.deviceMode->at(1), DeviceOption::Passthru);

    ASSERT_TRUE(merged.env);
    EXPECT_EQ(merged.env->size(), 3);
    EXPECT_EQ(merged.env->at("PATH"), "/usr/local/bin");
    EXPECT_EQ(merged.env->at("HOME"), "/home/user1");
    EXPECT_EQ(merged.env->at("USER"), "testuser");

    ASSERT_TRUE(merged.extDefs);
    EXPECT_EQ(merged.extDefs->size(), 2);

    EXPECT_EQ(merged.extDefs->at("app1").size(), 2);
    EXPECT_EQ(merged.extDefs->at("app1")[0].name, "extension1");
    EXPECT_EQ(merged.extDefs->at("app1")[1].name, "extension2");

    EXPECT_EQ(merged.extDefs->at("app2").size(), 1);
    EXPECT_EQ(merged.extDefs->at("app2")[0].name, "extension2");
}

TEST(RuntimeConfigTest, MergeEmptyConfigs)
{
    std::vector<RuntimeConfigure> empty_configs;
    auto merged = linglong::utils::MergeRuntimeConfig(empty_configs);

    EXPECT_FALSE(merged.deviceMode);
    EXPECT_FALSE(merged.env);
    EXPECT_FALSE(merged.extDefs);
}

TEST(RuntimeConfigTest, MergePartialConfigs)
{
    RuntimeConfigure config1;
    config1.deviceMode = std::vector<DeviceOption>{ DeviceOption::Passthru };
    config1.env = std::map<std::string, std::string>{ { "PATH", "/usr/bin" } };

    RuntimeConfigure config2;

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

    ASSERT_TRUE(merged.deviceMode);
    EXPECT_EQ(merged.deviceMode->size(), 1);
    EXPECT_EQ(merged.deviceMode->at(0), DeviceOption::Passthru);

    ASSERT_TRUE(merged.extDefs);
    EXPECT_EQ(merged.extDefs->size(), 1);
    EXPECT_EQ(merged.extDefs->at("app").size(), 1);
    EXPECT_EQ(merged.extDefs->at("app")[0].name, "test-ext");
}
