/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

using namespace semver;

TEST(VersionCompareTest, LessThanByNumbers)
{
    version v = version::parse("5.2.3");
    EXPECT_LT(v, version::parse("6.0.0"));
    EXPECT_LT(v, version::parse("5.3.3"));
    EXPECT_LT(v, version::parse("5.2.4"));
}

TEST(VersionCompareTest, LessThanByPrerelease)
{
    version v = version::parse("5.2.3-alpha.2");
    EXPECT_LT(v, version::parse("5.2.3-alpha.2.a"));
    EXPECT_LT(v, version::parse("5.2.3-alpha.3"));
    EXPECT_LT(v, version::parse("5.2.3-beta"));
    EXPECT_LE(v, version::parse("5.2.3-alpha.2"));
}

TEST(VersionCompareTest, CompareBySpec)
{
    EXPECT_LT(version::parse("1.0.0"), version::parse("2.0.0"));
    EXPECT_LT(version::parse("2.0.0"), version::parse("2.1.0"));
    EXPECT_LT(version::parse("2.1.0"), version::parse("2.1.1"));
    EXPECT_LT(version::parse("1.0.0-alpha"), version::parse("1.0.0"));
    EXPECT_LT(version::parse("1.0.0-alpha"), version::parse("1.0.0-alpha.1"));
    EXPECT_LT(version::parse("1.0.0-alpha.1"), version::parse("1.0.0-alpha.beta"));
    EXPECT_LT(version::parse("1.0.0-alpha.beta"), version::parse("1.0.0-beta"));
    EXPECT_LT(version::parse("1.0.0-beta"), version::parse("1.0.0-beta.2"));
    EXPECT_LT(version::parse("1.0.0-beta.2"), version::parse("1.0.0-beta.11"));
    EXPECT_LT(version::parse("1.0.0-beta.11"), version::parse("1.0.0-rc.1"));
    EXPECT_LT(version::parse("1.0.0-rc.1"), version::parse("1.0.0"));
}

TEST(VersionCompareTest, PrereleaseAlphabeticalComparison)
{
    EXPECT_LT(version::parse("5.2.3-alpha.2"), version::parse("5.2.3-alpha.a"));
    EXPECT_GT(version::parse("5.2.3-alpha.a"), version::parse("5.2.3-alpha.2"));
}

TEST(VersionCompareTest, PrereleaseAndStableComparison)
{
    EXPECT_LT(version::parse("5.2.3-alpha"), version::parse("5.2.3"));
    EXPECT_GT(version::parse("5.2.3"), version::parse("5.2.3-alpha"));
}

TEST(VersionCompareTest, GreaterThanByNumbers)
{
    version v = version::parse("5.2.3");
    EXPECT_GT(v, version::parse("4.0.0"));
    EXPECT_GT(v, version::parse("5.1.3"));
    EXPECT_GT(v, version::parse("5.2.2"));
    EXPECT_GE(v, version::parse("5.2.3"));
}

TEST(VersionCompareTest, GreaterThanByPrerelease)
{
    version v = version::parse("5.2.3-alpha.2");
    EXPECT_GT(v, version::parse("5.2.3-alpha"));
    EXPECT_GT(v, version::parse("5.2.3-alpha.1"));
    EXPECT_LT(v, version::parse("5.2.3-alpha.11"));
    EXPECT_GT(v, version::parse("5.2.3-a"));
    EXPECT_GE(v, version::parse("5.2.3-alpha.2"));
}

TEST(VersionCompareTest, Equality)
{
    EXPECT_EQ(version::parse("5.2.3-alpha.2"), version::parse("5.2.3-alpha.2"));
    EXPECT_NE(version::parse("5.2.3-alpha.2"), version::parse("5.2.3-alpha.5"));
    EXPECT_EQ(version::parse("5.2.3"), version::parse("5.2.3"));
    EXPECT_NE(version::parse("5.2.3"), version::parse("5.2.4"));
    EXPECT_EQ(version::parse("0.0.0"), version::parse("0.0.0"));
    EXPECT_EQ(version::parse("5.2.3-alpha.2+build.34"), version::parse("5.2.3-alpha.2"));
    EXPECT_EQ(version::parse("5.2.3-alpha.2+build.34"), version::parse("5.2.3-alpha.2+build.35"));
}

// 测试版本包含 security 字段
TEST(VersionCompareTest, SecurityFieldComparison)
{
    EXPECT_EQ(version::parse("5.2.3+security.1"), version::parse("5.2.3+security.1"));
    EXPECT_NE(version::parse("5.2.3+security.1"), version::parse("5.2.3+security.2"));
    EXPECT_LT(version::parse("5.2.3+security.1"), version::parse("5.2.3+security.2"));

    // 测试其他字段，确保 security 字段只影响比较
    EXPECT_LT(version::parse("5.2.3+security.1"), version::parse("5.2.3+security.2"));
    EXPECT_GT(version::parse("5.2.3+security.2"), version::parse("5.2.3+security.1"));
}

// 测试 security 字段与预发布版本组合
TEST(VersionCompareTest, SecurityFieldWithPrerelease)
{
    EXPECT_EQ(version::parse("5.2.3-alpha+security.1"), version::parse("5.2.3-alpha+security.1"));
    EXPECT_GE(version::parse("5.2.3-alpha+security.3"), version::parse("5.2.3-alpha+security.1"));
    EXPECT_GT(version::parse("5.2.3-alpha+security.2"), version::parse("5.2.3-alpha+security.1"));
    EXPECT_LE(version::parse("5.2.3-alpha+security.1"), version::parse("5.2.3-alpha+security.5"));
    EXPECT_LT(version::parse("1.0.0-alpha+security.3"), version::parse("1.0.0-alpha+security.4"));
    EXPECT_NE(version::parse("5.2.3-alpha+security.1"), version::parse("5.2.3-alpha+security.2"));
}

// 测试 security 字段与稳定版本的比较
TEST(VersionCompareTest, SecurityFieldWithStableVersion)
{
    EXPECT_GT(version::parse("5.2.3+security.1"), version::parse("5.2.3"));
    EXPECT_LT(version::parse("5.2.3"), version::parse("5.2.3+security.1"));
}

// 测试版本包含 buildinfo 和 security 字段
TEST(VersionCompareTest, BuildinfoSecurityFieldComparison)
{
    EXPECT_EQ(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0+buildinfo.security.1"));
    EXPECT_GE(version::parse("1.0.0+buildinfo.security.3"),
              version::parse("1.0.0+buildinfo.security.1"));
    EXPECT_GT(version::parse("1.0.0+buildinfo.security.2"),
              version::parse("1.0.0+buildinfo.security.1"));
    EXPECT_LE(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0+buildinfo.security.5"));
    EXPECT_LT(version::parse("1.0.0+buildinfo.security.2"),
              version::parse("1.0.0+buildinfo.security.10"));
    EXPECT_NE(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0+buildinfo.security.2"));
}

// 测试 buildinfo 和 security 字段与预发布版本组合
TEST(VersionCompareTest, BuildinfoSecurityFieldWithPrerelease)
{
    EXPECT_EQ(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
    EXPECT_GE(version::parse("1.0.0-alpha+buildinfo.security.3"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
    EXPECT_GT(version::parse("1.0.0-alpha+buildinfo.security.2"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
    EXPECT_LE(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.5"));
    EXPECT_LT(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.10"));
    EXPECT_NE(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.2"));
}

// 测试 buildinfo 和 security 字段与稳定版本的比较
TEST(VersionCompareTest, BuildinfoSecurityFieldWithStableVersion)
{
    EXPECT_GT(version::parse("1.0.0+buildinfo.security.1"), version::parse("1.0.0"));
    EXPECT_LT(version::parse("1.0.0"), version::parse("1.0.0+buildinfo.security.1"));
}

// 测试版本在其他字段（如前导数字、后缀等）混合使用 buildinfo 和 security 字段
TEST(VersionCompareTest, BuildinfoSecurityFieldWithMixedParts)
{
    EXPECT_LT(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.2"));
    EXPECT_LT(version::parse("1.0.0-alpha+buildinfo.security.2"),
              version::parse("1.0.0+buildinfo.security.1"));
    EXPECT_LE(version::parse("1.0.0-alpha+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
    EXPECT_GT(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
    EXPECT_GE(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0+buildinfo.security.1"));
    EXPECT_NE(version::parse("1.0.0+buildinfo.security.1"),
              version::parse("1.0.0-alpha+buildinfo.security.1"));
}
