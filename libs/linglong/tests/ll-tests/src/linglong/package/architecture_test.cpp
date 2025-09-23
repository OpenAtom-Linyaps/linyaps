/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/architecture.h"
#include "linglong/utils/strings.h"

using namespace linglong::package;

// 架构测试数据结构
struct ArchitectureTestData
{
    Architecture::Value value;
    const char *name;
    const char *triplet;
};

// 所有支持的架构测试数据
constexpr ArchitectureTestData ARCHITECTURE_TEST_DATA[] = {
    { Architecture::X86_64, "x86_64", "x86_64-linux-gnu" },
    { Architecture::ARM64, "arm64", "aarch64-linux-gnu" },
    { Architecture::LOONGARCH64, "loongarch64", "loongarch64-linux-gnu" },
    { Architecture::LOONG64, "loong64", "loongarch64-linux-gnu" },
    { Architecture::SW64, "sw64", "sw_64-linux-gnu" },
    { Architecture::MIPS64, "mips64", "mips64el-linux-gnuabi64" },
};

// 无效架构字符串测试数据
constexpr const char *INVALID_ARCHITECTURE_STRINGS[] = {
    "unknown", "invalid_arch", "x86", "amd64", "",
    "X86_64" // case sensitive
};

// 错误消息常量
constexpr auto ERROR_UNKNOWN_ARCHITECTURE = "unknow architecture";

TEST(Package, ArchitectureToString)
{
    // 使用for循环测试所有架构的toString()方法
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        EXPECT_EQ(Architecture(data.value).toStdString(), data.name);
    }
}

TEST(Package, ArchitectureGetTriplet)
{
    // 使用for循环测试所有架构的getTriplet()方法
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        EXPECT_EQ(Architecture(data.value).getTriplet(), data.triplet);
    }
}

TEST(Package, ArchitectureParseValid)
{
    // 使用for循环测试所有有效架构字符串的解析
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        auto result = Architecture::parse(data.name);
        ASSERT_TRUE(result.has_value()) << "Failed to parse: " << data.name;
        EXPECT_EQ(result->toStdString(), data.name);
        EXPECT_EQ(result->getTriplet(), data.triplet);
    }
}

TEST(Package, ArchitectureParseInvalid)
{
    // 使用for循环测试所有无效架构字符串的解析
    for (const auto *invalidStr : INVALID_ARCHITECTURE_STRINGS) {
        auto result = Architecture::parse(invalidStr);
        EXPECT_FALSE(result.has_value()) << "Should fail to parse: " << invalidStr;
    }
}

TEST(Package, ArchitectureConstructionFromString)
{
    // 使用for循环测试从字符串构造架构对象
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        Architecture arch(data.name);
        EXPECT_EQ(arch.toStdString(), data.name);
    }
}

TEST(Package, ArchitectureConstructionFromStringInvalid)
{
    // 测试无效字符串构造时抛出异常
    EXPECT_THROW(
      {
          try {
              Architecture arch(INVALID_ARCHITECTURE_STRINGS[0]);
          } catch (const std::runtime_error &e) {
              EXPECT_STREQ(e.what(), ERROR_UNKNOWN_ARCHITECTURE);
              throw;
          }
      },
      std::runtime_error);
}

TEST(Package, ArchitectureComparison)
{
    // 测试相等性和不等性运算符
    Architecture x86_64_1(Architecture::X86_64);
    Architecture x86_64_2(Architecture::X86_64);
    Architecture arm64(Architecture::ARM64);

    EXPECT_TRUE(x86_64_1 == x86_64_2);
    EXPECT_FALSE(x86_64_1 != x86_64_2);
    EXPECT_FALSE(x86_64_1 == arm64);
    EXPECT_TRUE(x86_64_1 != arm64);

    // 创建所有架构类型的数组
    std::vector<Architecture> architectures;
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        architectures.emplace_back(data.value);
    }

    // 每个架构应该等于自身而不等于其他架构
    for (size_t i = 0; i < architectures.size(); ++i) {
        for (size_t j = 0; j < architectures.size(); ++j) {
            if (i == j) {
                EXPECT_TRUE(architectures[i] == architectures[j]);
                EXPECT_FALSE(architectures[i] != architectures[j]);
            } else {
                EXPECT_FALSE(architectures[i] == architectures[j]);
                EXPECT_TRUE(architectures[i] != architectures[j]);
            }
        }
    }
}

TEST(Package, ArchitectureDefaultConstruction)
{
    // 测试默认构造
    Architecture defaultArch;
    EXPECT_EQ(defaultArch.toStdString(), "unknown"); // UNKNOW
    EXPECT_EQ(defaultArch.getTriplet(), "unknown");  // unknow
}

TEST(Package, ArchitectureCurrentCPUArchitecture)
{
    // 测试获取当前CPU架构
    auto currentArch = Architecture::currentCPUArchitecture();
    EXPECT_TRUE(currentArch.has_value()) << "should not fail (unless system problem)";
    // 这不应该失败（除非有系统问题）
    // 我们无法预测确切的架构，但可以验证它是有效的
    std::string archString = currentArch->toStdString();
    // 当前架构应该是支持的类型之一
    bool found = false;
    for (const auto &data : ARCHITECTURE_TEST_DATA) {
        if (archString == data.name) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Unknown architecture: " << archString;

    // 三元组不应为空，且应包含"linux-gnu"
    std::string triplet = currentArch->getTriplet();
    EXPECT_FALSE(triplet.empty());
    EXPECT_TRUE(linglong::utils::strings::contains(triplet, "linux-gnu"));
}