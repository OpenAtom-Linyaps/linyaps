/*
 ; SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <linglong/package/fallback_version.h>

using namespace linglong::package;

TEST(FallbackVersionTest, parse)
{
    auto result = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");

    result = FallbackVersion::parse("1.2.a");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "a");

    result = FallbackVersion::parse("1.2.3_alpha");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3_alpha");

    result = FallbackVersion::parse("1.2.3.4.5.6.7");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 7);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");
    EXPECT_EQ(result->list[3], "4");
    EXPECT_EQ(result->list[4], "5");
    EXPECT_EQ(result->list[5], "6");
    EXPECT_EQ(result->list[6], "7");

    result = FallbackVersion::parse("1.2.3@#$%^&*()");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3@#$%^&*()");

    result = FallbackVersion::parse("1.2.3 alpha");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3 alpha");

    result = FallbackVersion::parse("1.2.3-测试");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-测试");

    result = FallbackVersion::parse("1.2.3-😊");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-😊");

    EXPECT_FALSE(FallbackVersion::parse("").has_value());
    EXPECT_FALSE(FallbackVersion::parse("...").has_value());

    result = FallbackVersion::parse("1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 20);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3");
    EXPECT_EQ(result->list[3], "4");
    EXPECT_EQ(result->list[4], "5");
    EXPECT_EQ(result->list[5], "6");
    EXPECT_EQ(result->list[6], "7");
    EXPECT_EQ(result->list[7], "8");
    EXPECT_EQ(result->list[8], "9");
    EXPECT_EQ(result->list[9], "10");
    EXPECT_EQ(result->list[10], "11");
    EXPECT_EQ(result->list[11], "12");
    EXPECT_EQ(result->list[12], "13");
    EXPECT_EQ(result->list[13], "14");
    EXPECT_EQ(result->list[14], "15");
    EXPECT_EQ(result->list[15], "16");
    EXPECT_EQ(result->list[16], "17");
    EXPECT_EQ(result->list[17], "18");
    EXPECT_EQ(result->list[18], "19");
    EXPECT_EQ(result->list[19], "20");

    result = FallbackVersion::parse("1.2.3\n4\t5");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3\n4\t5");

    result = FallbackVersion::parse("1.2.3-日本語");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->list.size(), 3);
    EXPECT_EQ(result->list[0], "1");
    EXPECT_EQ(result->list[1], "2");
    EXPECT_EQ(result->list[2], "3-日本語");
}

// 测试兜底版本号比较
TEST(FallbackVersionTest, compare)
{
    // 包含字母的
    EXPECT_EQ(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.a").value());
    EXPECT_NE(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.b").value());
    EXPECT_LT(FallbackVersion::parse("1.2.a").value(), FallbackVersion::parse("1.2.b").value());
    EXPECT_GT(FallbackVersion::parse("1.2.b").value(), FallbackVersion::parse("1.2.a").value());
    // 包含特殊字符的版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_alpha").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_beta").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3_alpha").value(),
              FallbackVersion::parse("1.2.3_beta").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3_beta").value(),
              FallbackVersion::parse("1.2.3_alpha").value());
    // 超长版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.7").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.8").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3.4.5.6.7").value(),
              FallbackVersion::parse("1.2.3.4.5.6.8").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3.4.5.6.8").value(),
              FallbackVersion::parse("1.2.3.4.5.6.7").value());

    // 包含各种特殊字符的版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()_+").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3@#$%^&*()").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()_+").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3@#$%^&*()_+").value(),
              FallbackVersion::parse("1.2.3@#$%^&*()").value());

    // 包含空格的版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 alpha").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 beta").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3 alpha").value(),
              FallbackVersion::parse("1.2.3 beta").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3 beta").value(),
              FallbackVersion::parse("1.2.3 alpha").value());

    // 包含中文字符的版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-测试").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-版本").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3-测试").value(),
              FallbackVersion::parse("1.2.3-版本").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3-版本").value(),
              FallbackVersion::parse("1.2.3-测试").value());

    // 包含表情符号的版本号
    EXPECT_EQ(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-😊").value());
    EXPECT_NE(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-🚀").value());
    EXPECT_LT(FallbackVersion::parse("1.2.3-😊").value(),
              FallbackVersion::parse("1.2.3-🚀").value());
    EXPECT_GT(FallbackVersion::parse("1.2.3-🚀").value(),
              FallbackVersion::parse("1.2.3-😊").value());

    // 空字符串和只有分隔符的版本号
    EXPECT_FALSE(FallbackVersion::parse("").has_value());
    EXPECT_FALSE(FallbackVersion::parse("...").has_value());
    // 极端超长版本号
    EXPECT_TRUE(
      FallbackVersion::parse("1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20").has_value());

    // 2. 包含换行符和制表符的版本号
    EXPECT_TRUE(FallbackVersion::parse("1.2.3\n4\t5").has_value());

    // 3. 包含 Unicode 字符的版本号
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-日本語").has_value());
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-한국어").has_value());
    EXPECT_TRUE(FallbackVersion::parse("1.2.3-русский").has_value());
}
