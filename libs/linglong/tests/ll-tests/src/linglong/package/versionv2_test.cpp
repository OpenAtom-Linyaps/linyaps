/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/version.h"
#include "linglong/package/versionv2.h"

using namespace linglong::package;

TEST(VersionV2Test, ConstructorTest)
{
    VersionV2 v(1, 2, 3, "alpha", "build123", 4);

    EXPECT_EQ(v.major, 1);
    EXPECT_EQ(v.minor, 2);
    EXPECT_EQ(v.patch, 3);
    EXPECT_EQ(v.prerelease, "alpha");
    EXPECT_EQ(v.buildMeta, "build123");
    EXPECT_EQ(v.security, 4);
}

// 测试 == 运算符
TEST(VersionV2Test, EqualityOperatorTest)
{
    VersionV2 v1(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v3(1, 2, 3, "beta", "build123", 4);

    EXPECT_TRUE(v1 == v2);
    EXPECT_FALSE(v1 == v3);
}

// 测试 != 运算符
TEST(VersionV2Test, InequalityOperatorTest)
{
    VersionV2 v1(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 3, "beta", "build123", 4);

    EXPECT_TRUE(v1 != v2);
    EXPECT_FALSE(v1 != v1);
}

// 测试 < 运算符
TEST(VersionV2Test, LessThanOperatorTest)
{
    VersionV2 v1(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 4, "alpha", "build123", 4);
    VersionV2 v3(1, 2, 3, "beta", "build123", 4);

    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v1 < v3);
    EXPECT_FALSE(v2 < v1);
}

// 测试 > 运算符
TEST(VersionV2Test, GreaterThanOperatorTest)
{
    VersionV2 v1(1, 2, 4, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v3(1, 2, 3, "beta", "build123", 4);

    EXPECT_TRUE(v1 > v2);
    EXPECT_TRUE(v1 > v3);
    EXPECT_FALSE(v2 > v1);
}

// 测试 <= 运算符
TEST(VersionV2Test, LessThanOrEqualOperatorTest)
{
    VersionV2 v1(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v3(1, 2, 4, "alpha", "build123", 4);

    EXPECT_TRUE(v1 <= v2);
    EXPECT_TRUE(v1 <= v3);
    EXPECT_FALSE(v3 <= v1);
}

// 测试 >= 运算符
TEST(VersionV2Test, GreaterThanOrEqualOperatorTest)
{
    VersionV2 v1(1, 2, 4, "alpha", "build123", 4);
    VersionV2 v2(1, 2, 3, "alpha", "build123", 4);
    VersionV2 v3(1, 2, 4, "alpha", "build123", 4);

    EXPECT_TRUE(v1 >= v2);
    EXPECT_TRUE(v1 >= v3);
    EXPECT_FALSE(v2 >= v1);
}

// 测试与 Version 类的 != 运算符
TEST(VersionV2Test, InequalityWithVersionTest)
{
    EXPECT_NE(VersionV1::parse("1.2.3.0").value(), VersionV2::parse("1.2.3").value());
    EXPECT_NE(VersionV1::parse("1.2.3").value(), VersionV2::parse("1.2.3-alpha+build123").value());
    EXPECT_NE(VersionV1::parse("1.2.3").value(),
              VersionV2::parse("1.2.3-alpha+build123.security.2").value());
    EXPECT_NE(VersionV1::parse("1.2.3").value(), VersionV2::parse("1.2.3-alpha").value());
    EXPECT_NE(VersionV1::parse("1.2.3").value(),
              VersionV2::parse("1.2.3+build123.security.2").value());
    EXPECT_NE(VersionV1::parse("1.2.3").value(), VersionV2::parse("1.2.3+security.2").value());
}

// 测试与 Version 类的 < 运算符
TEST(VersionV2Test, LessThanWithVersionTest)
{
    EXPECT_LT(VersionV2::parse("1.2.3-alpha+build123").value(), VersionV1::parse("1.2.4").value());
    EXPECT_LT(VersionV2::parse("1.2.3-alpha+build123.security.2").value(),
              VersionV1::parse("1.2.4.0").value());
    EXPECT_LT(VersionV2::parse("1.2.3+build123").value(), VersionV1::parse("1.2.4.0").value());
    EXPECT_LT(VersionV2::parse("1.2.4-alpha").value(), VersionV1::parse("1.2.4.0").value());
    EXPECT_LT(VersionV2::parse("1.2.4").value(), VersionV1::parse("1.2.4.0").value());
}

// 测试与 Version 类的 > 运算符
TEST(VersionV2Test, GreaterThanWithVersionTest)
{
    EXPECT_GT(VersionV2::parse("1.2.4-alpha+build123").value(), VersionV1::parse("1.2.3").value());
    EXPECT_GT(VersionV2::parse("1.2.4-alpha+build123").value(),
              VersionV1::parse("1.2.3.0").value());
    EXPECT_GT(VersionV2::parse("1.2.4+build123").value(), VersionV1::parse("1.2.3.1").value());
    EXPECT_GT(VersionV2::parse("1.2.4+build123.security.1").value(),
              VersionV1::parse("1.2.3").value());
    EXPECT_GT(VersionV2::parse("1.2.4+build.1.security.2").value(),
              VersionV1::parse("1.2.4").value());
    EXPECT_GT(VersionV1::parse("1.2.4.0").value(),
              VersionV2::parse("1.2.4+build123.security.1").value());
}

// 测试与 Version 类的 <= 运算符
TEST(VersionV2Test, LessThanOrEqualWithVersionTest)
{
    EXPECT_LE(VersionV2::parse("2.6.8-beta").value(), VersionV1::parse("2.6.8").value());
    EXPECT_LE(VersionV2::parse("2.6.7").value(), VersionV1::parse("2.6.8").value());
    EXPECT_LE(VersionV2::parse("2.6.8+build.1").value(), VersionV1::parse("2.6.8").value());
    EXPECT_LE(VersionV2::parse("2.6.8-beta").value(), VersionV1::parse("2.6.8").value());
    EXPECT_LE(VersionV2::parse("2.6.8").value(), VersionV1::parse("2.6.8").value());
}

// 测试与 Version 类的 >= 运算符
TEST(VersionV2Test, GreaterThanOrEqualWithVersionTest)
{
    EXPECT_GE(VersionV2::parse("2.6.8").value(), VersionV1::parse("2.6.8").value());
    EXPECT_GE(VersionV2::parse("2.6.7+security.2").value(), VersionV1::parse("2.6.7").value());
    EXPECT_GE(VersionV2::parse("2.6.8+build.1").value(), VersionV1::parse("2.6.8").value());
    EXPECT_GE(VersionV2::parse("2.6.9-alpha.1").value(), VersionV1::parse("2.6.8").value());
    EXPECT_GE(VersionV2::parse("2.6.9-beta.2").value(), VersionV1::parse("2.6.8").value());
}

// 测试 parse 函数
TEST(VersionV2Test, ParseTest)
{
    auto result = VersionV2::parse("1.2.3-alpha+build123.security.4");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_EQ(result->prerelease, "alpha");
    EXPECT_EQ(result->buildMeta, "build123.security.4");
    EXPECT_EQ(result->security, 4);

    result = VersionV2::parse("1.2.3-alpha+build123");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_EQ(result->prerelease, "alpha");
    EXPECT_EQ(result->buildMeta, "build123");
    EXPECT_EQ(result->security, 0);

    result = VersionV2::parse("1.2.3-alpha");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_EQ(result->prerelease, "alpha");
    EXPECT_EQ(result->buildMeta, "");
    EXPECT_EQ(result->security, 0);

    result = VersionV2::parse("1.2.3");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_EQ(result->prerelease, "");
    EXPECT_EQ(result->buildMeta, "");
    EXPECT_EQ(result->security, 0);

    result = VersionV2::parse("1.2.3+security.4");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->major, 1);
    EXPECT_EQ(result->minor, 2);
    EXPECT_EQ(result->patch, 3);
    EXPECT_EQ(result->prerelease, "");
    EXPECT_EQ(result->buildMeta, "security.4");
    EXPECT_EQ(result->security, 4);

    auto invalidResult = VersionV2::parse("invalid-version");
    EXPECT_FALSE(invalidResult.has_value());
}
