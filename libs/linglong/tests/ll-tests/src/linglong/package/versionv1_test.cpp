/*
 * SPDX-FileCopyrightText: 2026 Yanghanrui666
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"

using namespace linglong::package;

// 测试构造函数（三位版本号）
TEST(VersionV1Test, ConstructorThreeDigits)
{
    VersionV1 v("1.2.3");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
    EXPECT_FALSE(v.tweak.has_value());
}

// 测试构造函数（四位版本号）
TEST(VersionV1Test, ConstructorFourDigits)
{
    VersionV1 v("1.2.3.4");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
    ASSERT_TRUE(v.tweak.has_value());
    EXPECT_EQ(*v.tweak, 4);
}

// 测试构造函数（零值版本号）
TEST(VersionV1Test, ConstructorZeros)
{
    VersionV1 v("0.0.0");
    EXPECT_EQ(v.major, 0);
    EXPECT_EQ(v.minor, 0);
    EXPECT_EQ(v.patch, 0);
    EXPECT_FALSE(v.tweak.has_value());
}

// 测试构造函数（四位版本号，tweak 为 0）
TEST(VersionV1Test, ConstructorTweakZero)
{
    VersionV1 v("1.2.3.0");
    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
    ASSERT_TRUE(v.tweak.has_value());
    EXPECT_EQ(*v.tweak, 0);
}

// 测试 parse 函数（有效输入）
TEST(VersionV1Test, ParseValid)
{
    auto result = VersionV1::parse("1.2.3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_FALSE(result->tweak.has_value());

    result = VersionV1::parse("1.2.3.4");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    ASSERT_TRUE(result->tweak.has_value());
    EXPECT_EQ(*result->tweak, 4);

    result = VersionV1::parse("0.0.0");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 0);
    EXPECT_EQ(result->minor, 0);
    EXPECT_EQ(result->patch, 0);

    result = VersionV1::parse("10.20.30.40");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 10);
    EXPECT_EQ(result->minor, 20);
    EXPECT_EQ(result->patch, 30);
    ASSERT_TRUE(result->tweak.has_value());
    EXPECT_EQ(*result->tweak, 40);
}

// 测试 parse 函数（无效输入）
TEST(VersionV1Test, ParseInvalid)
{
    EXPECT_FALSE(VersionV1::parse("invalid").has_value());
    EXPECT_FALSE(VersionV1::parse("1.2").has_value());
    EXPECT_FALSE(VersionV1::parse("1.2.3.4.5").has_value());
    EXPECT_FALSE(VersionV1::parse("1.2.3-alpha").has_value());
    EXPECT_FALSE(VersionV1::parse("01.2.3").has_value());
    EXPECT_FALSE(VersionV1::parse("1.02.3").has_value());
    EXPECT_FALSE(VersionV1::parse("1.2.03").has_value());
    EXPECT_FALSE(VersionV1::parse("").has_value());
    EXPECT_FALSE(VersionV1::parse("1.2.3.").has_value());
    EXPECT_FALSE(VersionV1::parse(".1.2.3").has_value());
    EXPECT_FALSE(VersionV1::parse("1..2.3").has_value());
}

// 测试 toString 函数
TEST(VersionV1Test, ToString)
{
    EXPECT_EQ(VersionV1("1.2.3").toString(), "1.2.3");
    EXPECT_EQ(VersionV1("1.2.3.4").toString(), "1.2.3.4");
    EXPECT_EQ(VersionV1("0.0.0").toString(), "0.0.0");
    EXPECT_EQ(VersionV1("0.0.0.0").toString(), "0.0.0.0");
    EXPECT_EQ(VersionV1("10.20.30.40").toString(), "10.20.30.40");
}

// 测试 semanticMatch 函数
TEST(VersionV1Test, SemanticMatch)
{
    VersionV1 v("1.2.3");

    EXPECT_TRUE(v.semanticMatch("1.2.3"));
    EXPECT_TRUE(v.semanticMatch("1.2.3.4")); // tweak 不参与匹配（当自身无 tweak 时）
    EXPECT_FALSE(v.semanticMatch("1.2.4"));
    EXPECT_FALSE(v.semanticMatch("1.3.3"));
    EXPECT_FALSE(v.semanticMatch("2.2.3"));
    EXPECT_FALSE(v.semanticMatch("invalid"));
}

// 测试 semanticMatch 函数（自身有 tweak 的情况）
TEST(VersionV1Test, SemanticMatchWithTweak)
{
    VersionV1 v("1.2.3.4");

    EXPECT_TRUE(v.semanticMatch("1.2.3.4"));  // 双方都有 tweak 且相同
    EXPECT_TRUE(v.semanticMatch("1.2.3"));    // 对方无 tweak，不比较 tweak
    EXPECT_FALSE(v.semanticMatch("1.2.3.5")); // 双方都有 tweak 但不同
    EXPECT_FALSE(v.semanticMatch("1.2.4.4"));
    EXPECT_FALSE(v.semanticMatch("invalid"));
}

// 测试 == 运算符
TEST(VersionV1Test, EqualityOperator)
{
    EXPECT_TRUE(VersionV1("1.2.3") == VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.2.3.4") == VersionV1("1.2.3.4"));
    EXPECT_FALSE(VersionV1("1.2.3") == VersionV1("1.2.4"));
    EXPECT_FALSE(VersionV1("1.2.3") == VersionV1("1.3.3"));
    EXPECT_FALSE(VersionV1("1.2.3") == VersionV1("2.2.3"));

    // tweak 存在性不同时，== 返回 false（即使数值相同）
    EXPECT_FALSE(VersionV1("1.2.3") == VersionV1("1.2.3.0"));
    EXPECT_FALSE(VersionV1("1.2.3.0") == VersionV1("1.2.3"));
}

// 测试 != 运算符
TEST(VersionV1Test, InequalityOperator)
{
    EXPECT_FALSE(VersionV1("1.2.3") != VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.2.3") != VersionV1("1.2.4"));
    EXPECT_TRUE(VersionV1("1.2.3") != VersionV1("1.2.3.0"));
    EXPECT_TRUE(VersionV1("1.2.3.0") != VersionV1("1.2.3"));
}

// 测试 < 运算符
TEST(VersionV1Test, LessThanOperator)
{
    EXPECT_TRUE(VersionV1("1.2.3") < VersionV1("1.2.4"));
    EXPECT_TRUE(VersionV1("1.2.3") < VersionV1("1.3.0"));
    EXPECT_TRUE(VersionV1("1.2.3") < VersionV1("2.0.0"));
    EXPECT_FALSE(VersionV1("1.2.3") < VersionV1("1.2.3"));

    // tweak 使用 value_or(0) 比较，所以 1.2.3 不小于 1.2.3.0
    EXPECT_FALSE(VersionV1("1.2.3") < VersionV1("1.2.3.0"));
    EXPECT_FALSE(VersionV1("1.2.3.0") < VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.2.3.0") < VersionV1("1.2.3.1"));
}

// 测试 > 运算符
TEST(VersionV1Test, GreaterThanOperator)
{
    EXPECT_TRUE(VersionV1("1.2.4") > VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.3.0") > VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("2.0.0") > VersionV1("1.2.3"));
    EXPECT_FALSE(VersionV1("1.2.3") > VersionV1("1.2.3"));
    EXPECT_FALSE(VersionV1("1.2.3") > VersionV1("1.2.3.0"));
    EXPECT_TRUE(VersionV1("1.2.3.1") > VersionV1("1.2.3.0"));
}

// 测试 <= 运算符
TEST(VersionV1Test, LessThanOrEqualOperator)
{
    EXPECT_TRUE(VersionV1("1.2.3") <= VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.2.3") <= VersionV1("1.2.4"));
    EXPECT_FALSE(VersionV1("1.2.4") <= VersionV1("1.2.3"));

    // tweak 存在性不同：== 和 < 均为 false，所以 <= 也不成立
    EXPECT_FALSE(VersionV1("1.2.3") <= VersionV1("1.2.3.0"));
    EXPECT_FALSE(VersionV1("1.2.3.0") <= VersionV1("1.2.3"));
}

// 测试 >= 运算符
TEST(VersionV1Test, GreaterThanOrEqualOperator)
{
    EXPECT_TRUE(VersionV1("1.2.3") >= VersionV1("1.2.3"));
    EXPECT_TRUE(VersionV1("1.2.4") >= VersionV1("1.2.3"));
    EXPECT_FALSE(VersionV1("1.2.3") >= VersionV1("1.2.4"));

    // tweak 存在性不同：== 和 > 均为 false，所以 >= 也不成立
    EXPECT_FALSE(VersionV1("1.2.3") >= VersionV1("1.2.3.0"));
    EXPECT_FALSE(VersionV1("1.2.3.0") >= VersionV1("1.2.3"));
}

// 测试与 VersionV2 的 == 运算符
TEST(VersionV1Test, EqualityWithVersionV2)
{
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() == VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3.0").value() == VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() == VersionV2::parse("1.2.3-alpha").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() == VersionV2::parse("1.2.3+security.1").value());
    EXPECT_FALSE(VersionV1::parse("1.2.4").value() == VersionV2::parse("1.2.3").value());
}

// 测试与 VersionV2 的 < 运算符
TEST(VersionV1Test, LessThanWithVersionV2)
{
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() < VersionV2::parse("1.2.4").value());
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() < VersionV2::parse("1.2.3+security.1").value());
    EXPECT_FALSE(VersionV1::parse("1.2.4").value() < VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() < VersionV2::parse("1.2.3").value());
}

// 测试与 VersionV2 的 > 运算符
TEST(VersionV1Test, GreaterThanWithVersionV2)
{
    EXPECT_TRUE(VersionV1::parse("1.2.4").value() > VersionV2::parse("1.2.3").value());
    EXPECT_TRUE(VersionV1::parse("1.2.3.1").value() > VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() > VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() > VersionV2::parse("1.2.4").value());
}

// 测试与 VersionV2 的 <= 运算符
TEST(VersionV1Test, LessThanOrEqualWithVersionV2)
{
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() <= VersionV2::parse("1.2.3").value());
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() <= VersionV2::parse("1.2.4").value());
    EXPECT_FALSE(VersionV1::parse("1.2.4").value() <= VersionV2::parse("1.2.3").value());
}

// 测试与 VersionV2 的 >= 运算符
TEST(VersionV1Test, GreaterThanOrEqualWithVersionV2)
{
    EXPECT_TRUE(VersionV1::parse("1.2.3").value() >= VersionV2::parse("1.2.3").value());
    EXPECT_TRUE(VersionV1::parse("1.2.4").value() >= VersionV2::parse("1.2.3").value());
    EXPECT_FALSE(VersionV1::parse("1.2.3").value() >= VersionV2::parse("1.2.4").value());
}
