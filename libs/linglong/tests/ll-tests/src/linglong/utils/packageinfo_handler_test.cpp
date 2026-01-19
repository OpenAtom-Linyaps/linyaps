// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"
#include "linglong/utils/serialize/packageinfo_handler.h"

#include <filesystem>
#include <fstream>

using namespace linglong::utils;

class PackageInfoHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        char tempPath[] = "/var/tmp/linglong-uab-file-test-XXXXXX";
        tempDir = mkdtemp(tempPath);
        ASSERT_FALSE(tempDir.empty()) << "Failed to create temporary directory";
    }

    void TearDown() override
    {
        // 清理临时目录
        std::filesystem::remove_all(tempDir);
    }

    std::filesystem::path tempDir;
};

TEST_F(PackageInfoHandlerTest, toPackageInfoV2_BasicConversion)
{
    // 创建PackageInfo对象
    linglong::api::types::v1::PackageInfo oldInfo;
    oldInfo.appid = "com.example.testapp";
    oldInfo.arch = { "x86_64" };
    oldInfo.base = "org.deepin.base";
    oldInfo.channel = "main";
    oldInfo.command = std::vector<std::string>{ "testapp" };
    oldInfo.description = "Test application";
    oldInfo.kind = "app";
    oldInfo.packageInfoModule = "runtime";
    oldInfo.name = "TestApp";
    oldInfo.size = 1024;
    oldInfo.version = "1.0.0";

    // 转换为PackageInfoV2
    auto result = linglong::utils::serialize::toPackageInfoV2(oldInfo);

    // 验证转换结果
    EXPECT_EQ(result.id, oldInfo.appid);
    EXPECT_EQ(result.arch, oldInfo.arch);
    EXPECT_EQ(result.base, oldInfo.base);
    EXPECT_EQ(result.channel, "main"); // channel有默认值
    EXPECT_EQ(result.command, oldInfo.command);
    EXPECT_EQ(result.description, oldInfo.description);
    EXPECT_EQ(result.kind, oldInfo.kind);
    EXPECT_EQ(result.packageInfoV2Module, oldInfo.packageInfoModule);
    EXPECT_EQ(result.name, oldInfo.name);
    // 比较permissions - 两者都应该是空值
    EXPECT_EQ(result.permissions.has_value(), oldInfo.permissions.has_value());
    EXPECT_EQ(result.runtime, oldInfo.runtime);
    EXPECT_EQ(result.schemaVersion, PACKAGE_INFO_VERSION);
    EXPECT_EQ(result.size, oldInfo.size);
    EXPECT_EQ(result.version, oldInfo.version);

    // 验证可选字段
    EXPECT_FALSE(result.compatibleVersion.has_value());
    EXPECT_FALSE(result.uuid.has_value());
}

TEST_F(PackageInfoHandlerTest, toPackageInfoV2_WithOptionalFields)
{
    // 创建包含可选字段的PackageInfo对象
    linglong::api::types::v1::PackageInfo oldInfo;
    oldInfo.appid = "com.example.testapp";
    oldInfo.arch = { "x86_64" };
    oldInfo.base = "org.deepin.base";
    oldInfo.channel = std::nullopt;     // 空channel
    oldInfo.command = std::nullopt;     // 空command
    oldInfo.description = std::nullopt; // 空description
    oldInfo.kind = "app";
    oldInfo.packageInfoModule = "runtime";
    oldInfo.name = "TestApp";
    oldInfo.permissions = std::nullopt; // 空permissions
    oldInfo.runtime = std::nullopt;     // 空runtime
    oldInfo.size = 2048;
    oldInfo.version = "2.0.0";

    // 转换为PackageInfoV2
    auto result = linglong::utils::serialize::toPackageInfoV2(oldInfo);

    // 验证转换结果
    EXPECT_EQ(result.id, oldInfo.appid);
    EXPECT_EQ(result.arch, oldInfo.arch);
    EXPECT_EQ(result.base, oldInfo.base);
    EXPECT_EQ(result.channel, "main");            // 空channel使用默认值
    EXPECT_FALSE(result.command.has_value());     // 空command保持空
    EXPECT_FALSE(result.description.has_value()); // 空description保持空
    EXPECT_EQ(result.kind, oldInfo.kind);
    EXPECT_EQ(result.packageInfoV2Module, oldInfo.packageInfoModule);
    EXPECT_EQ(result.name, oldInfo.name);
    EXPECT_FALSE(result.permissions.has_value()); // 空permissions保持空
    EXPECT_FALSE(result.runtime.has_value());     // 空runtime保持空
    EXPECT_EQ(result.schemaVersion, PACKAGE_INFO_VERSION);
    EXPECT_EQ(result.size, oldInfo.size);
    EXPECT_EQ(result.version, oldInfo.version);
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromFileV2)
{
    // 创建PackageInfoV2 JSON文件
    std::string jsonContent = R"({
        "arch": ["x86_64"],
        "base": "org.deepin.base",
        "channel": "main",
        "command": ["testapp"],
        "description": "Test application",
        "id": "com.example.testapp",
        "kind": "app",
        "module": "runtime",
        "name": "TestApp",
        "schema_version": "1.0",
        "size": 1024,
        "version": "1.0.0"
    })";

    std::filesystem::path filePath = tempDir / "package_v2.json";
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file << jsonContent;
    file.close();

    // 解析文件
    auto result = linglong::utils::serialize::parsePackageInfoFile(filePath);

    // 验证解析结果
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->id, "com.example.testapp");
    EXPECT_EQ(result->arch, std::vector<std::string>{ "x86_64" });
    EXPECT_EQ(result->base, "org.deepin.base");
    EXPECT_EQ(result->channel, "main");
    EXPECT_EQ(result->command, std::vector<std::string>{ "testapp" });
    EXPECT_EQ(result->description, "Test application");
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->packageInfoV2Module, "runtime");
    EXPECT_EQ(result->name, "TestApp");
    EXPECT_EQ(result->schemaVersion, "1.0");
    EXPECT_EQ(result->size, 1024);
    EXPECT_EQ(result->version, "1.0.0");
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromFileV1)
{
    // 创建PackageInfo JSON文件（V1格式）
    std::string jsonContent = R"({
        "appid": "com.example.testapp",
        "arch": ["x86_64"],
        "base": "org.deepin.base",
        "channel": "main",
        "command": ["testapp"],
        "description": "Test application",
        "kind": "app",
        "module": "runtime",
        "name": "TestApp",
        "size": 1024,
        "version": "1.0.0"
    })";

    std::filesystem::path filePath = tempDir / "package_v1.json";
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file << jsonContent;
    file.close();

    // 解析文件
    auto result = linglong::utils::serialize::parsePackageInfoFile(filePath);

    // 验证解析结果
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_EQ(result->id, "com.example.testapp");
    EXPECT_EQ(result->arch, std::vector<std::string>{ "x86_64" });
    EXPECT_EQ(result->base, "org.deepin.base");
    EXPECT_EQ(result->channel, "main");
    EXPECT_EQ(result->command, std::vector<std::string>{ "testapp" });
    EXPECT_EQ(result->description, "Test application");
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->packageInfoV2Module, "runtime");
    EXPECT_EQ(result->name, "TestApp");
    EXPECT_EQ(result->schemaVersion, PACKAGE_INFO_VERSION);
    EXPECT_EQ(result->size, 1024);
    EXPECT_EQ(result->version, "1.0.0");
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromFileInvalid)
{
    // 创建无效的JSON文件
    std::string jsonContent = R"({
        "invalid": "json content"
    })";

    std::filesystem::path filePath = tempDir / "invalid.json";
    std::ofstream file(filePath);
    ASSERT_TRUE(file.is_open());
    file << jsonContent;
    file.close();

    // 解析文件
    auto result = linglong::utils::serialize::parsePackageInfoFile(filePath);

    // 验证解析失败
    EXPECT_FALSE(result.has_value());
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromJSONV2)
{
    // 创建PackageInfoV2 JSON对象
    nlohmann::json jsonData = { { "arch", { "x86_64" } },
                                { "base", "org.deepin.base" },
                                { "channel", "main" },
                                { "command", { "testapp" } },
                                { "description", "Test application" },
                                { "id", "com.example.testapp" },
                                { "kind", "app" },
                                { "module", "runtime" },
                                { "name", "TestApp" },
                                { "schema_version", "1.0" },
                                { "size", 1024 },
                                { "version", "1.0.0" } };

    // 解析JSON
    auto result = linglong::utils::serialize::parsePackageInfo(jsonData);

    // 验证解析结果
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->id, "com.example.testapp");
    EXPECT_EQ(result->arch, std::vector<std::string>{ "x86_64" });
    EXPECT_EQ(result->base, "org.deepin.base");
    EXPECT_EQ(result->channel, "main");
    EXPECT_EQ(result->command, std::vector<std::string>{ "testapp" });
    EXPECT_EQ(result->description, "Test application");
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->packageInfoV2Module, "runtime");
    EXPECT_EQ(result->name, "TestApp");
    EXPECT_EQ(result->schemaVersion, "1.0");
    EXPECT_EQ(result->size, 1024);
    EXPECT_EQ(result->version, "1.0.0");
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromJSONV1)
{
    // 创建PackageInfo JSON对象（V1格式）
    nlohmann::json jsonData = { { "appid", "com.example.testapp" },
                                { "arch", { "x86_64" } },
                                { "base", "org.deepin.base" },
                                { "channel", "main" },
                                { "command", { "testapp" } },
                                { "description", "Test application" },
                                { "kind", "app" },
                                { "module", "runtime" },
                                { "name", "TestApp" },
                                { "size", 1024 },
                                { "version", "1.0.0" } };

    // 解析JSON
    auto result = linglong::utils::serialize::parsePackageInfo(jsonData);

    // 验证解析结果
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_EQ(result->id, "com.example.testapp");
    EXPECT_EQ(result->arch, std::vector<std::string>{ "x86_64" });
    EXPECT_EQ(result->base, "org.deepin.base");
    EXPECT_EQ(result->channel, "main");
    EXPECT_EQ(result->command, std::vector<std::string>{ "testapp" });
    EXPECT_EQ(result->description, "Test application");
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->packageInfoV2Module, "runtime");
    EXPECT_EQ(result->name, "TestApp");
    EXPECT_EQ(result->schemaVersion, PACKAGE_INFO_VERSION);
    EXPECT_EQ(result->size, 1024);
    EXPECT_EQ(result->version, "1.0.0");
}

TEST_F(PackageInfoHandlerTest, parsePackageInfo_FromJSONInvalid)
{
    // 创建无效的JSON对象
    nlohmann::json jsonData = { { "invalid", "json content" } };

    // 解析JSON
    auto result = linglong::utils::serialize::parsePackageInfo(jsonData);

    // 验证解析失败
    EXPECT_FALSE(result.has_value());
}
