// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"
#include "linglong/utils/xdg/directory.h"

#include <cstdlib>
#include <filesystem>

TEST(XDGDirectory, appDataDirWithHOME)
{
    // 保存原始HOME环境变量
    const char *originalHome = std::getenv("HOME");

    // 设置测试用的HOME环境变量
    const char *testHome = "/tmp/test_home";
    setenv("HOME", testHome, 1);

    // 测试appDataDir函数
    const std::string appID = "com.example.testapp";
    auto result = linglong::utils::xdg::appDataDir(appID);

    // 验证结果
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{ testHome } / ".linglong" / appID);

    // 恢复原始HOME环境变量
    if (originalHome != nullptr) {
        setenv("HOME", originalHome, 1);
    } else {
        unsetenv("HOME");
    }
}

TEST(XDGDirectory, appDataDirWithoutHOME)
{
    // 保存原始HOME环境变量
    const char *originalHome = std::getenv("HOME");

    // 移除HOME环境变量
    unsetenv("HOME");

    // 测试appDataDir函数
    const std::string appID = "com.example.testapp";
    auto result = linglong::utils::xdg::appDataDir(appID);

    // 验证结果应该是错误
    EXPECT_FALSE(result.has_value());
    // 错误消息包含完整的跟踪信息，我们只需要检查是否包含"HOME is not set"
    EXPECT_TRUE(result.error().message().find("HOME is not set") != std::string::npos);

    // 恢复原始HOME环境变量
    if (originalHome != nullptr) {
        setenv("HOME", originalHome, 1);
    }
}

TEST(XDGDirectory, appDataDirWithEmptyAppID)
{
    // 保存原始HOME环境变量
    const char *originalHome = std::getenv("HOME");

    // 设置测试用的HOME环境变量
    const char *testHome = "/tmp/test_home";
    setenv("HOME", testHome, 1);

    // 测试空appID
    const std::string appID = "";
    auto result = linglong::utils::xdg::appDataDir(appID);

    // 验证结果
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{ testHome } / ".linglong" / "");

    // 恢复原始HOME环境变量
    if (originalHome != nullptr) {
        setenv("HOME", originalHome, 1);
    } else {
        unsetenv("HOME");
    }
}

TEST(XDGDirectory, appDataDirWithComplexAppID)
{
    // 保存原始HOME环境变量
    const char *originalHome = std::getenv("HOME");

    // 设置测试用的HOME环境变量
    const char *testHome = "/tmp/test_home";
    setenv("HOME", testHome, 1);

    // 测试复杂的appID
    const std::string appID = "org.deepin.linglong.example";
    auto result = linglong::utils::xdg::appDataDir(appID);

    // 验证结果
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{ testHome } / ".linglong" / appID);

    // 恢复原始HOME环境变量
    if (originalHome != nullptr) {
        setenv("HOME", originalHome, 1);
    } else {
        unsetenv("HOME");
    }
}
