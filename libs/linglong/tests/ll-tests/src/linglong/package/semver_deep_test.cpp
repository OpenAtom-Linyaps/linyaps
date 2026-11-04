/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/fallback_version.h"
#include "linglong/package/version.h"
#include "linglong/package/versionv1.h"
#include "linglong/package/versionv2.h"

#include <string>
#include <vector>

namespace linglong::package {
namespace {

TEST(SemverDeepTest, VersionV1BasicParsingAndSerialization)
{
    auto res = VersionV1::parse("1.2.3.4");
    ASSERT_TRUE(res.has_value()) << res.error().message();
    EXPECT_EQ(res->toString(), "1.2.3.4");
}

TEST(SemverDeepTest, VersionV1ComparisonOperators)
{
    auto v1 = VersionV1::parse("1.0.0.0");
    auto v2 = VersionV1::parse("1.0.0.1");
    auto v3 = VersionV1::parse("2.0.0.0");

    ASSERT_TRUE(v1 && v2 && v3);
    EXPECT_LT(*v1, *v2);
    EXPECT_LT(*v2, *v3);
    EXPECT_GT(*v3, *v1);
    EXPECT_EQ(*v1, *v1);
}

TEST(SemverDeepTest, VersionV2SemverSpecConformance)
{
    auto v1 = VersionV2::parse("1.2.3-alpha.1+build.100");
    ASSERT_TRUE(v1.has_value()) << v1.error().message();
    EXPECT_EQ(v1->toString(), "1.2.3-alpha.1+build.100");

    auto v2 = VersionV2::parse("1.2.3-beta.1");
    ASSERT_TRUE(v2.has_value());

    EXPECT_LT(*v1, *v2);
}

TEST(SemverDeepTest, VersionV2PrereleasePrecedenceRules)
{
    std::vector<std::string> versions = { "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-alpha.beta",
                                          "1.0.0-beta",  "1.0.0-beta.2",  "1.0.0-rc.1",
                                          "1.0.0" };

    for (size_t i = 0; i < versions.size() - 1; ++i) {
        auto va = VersionV2::parse(versions[i]);
        auto vb = VersionV2::parse(versions[i + 1]);
        ASSERT_TRUE(va && vb) << "Failed to parse: " << versions[i] << " or " << versions[i + 1];
        EXPECT_LT(*va, *vb) << versions[i] << " should be less than " << versions[i + 1];
    }
}

TEST(SemverDeepTest, InvalidVersionStringsRejection)
{
    std::vector<std::string> badVersions = { "invalid_version", "1.2.3.4.5.6", "a.b.c", "-1.0.0" };

    for (const auto &str : badVersions) {
        auto v1Res = VersionV1::parse(str);
        auto v2Res = VersionV2::parse(str);
        EXPECT_FALSE(v1Res && v2Res) << "Should fail for: " << str;
    }
}

TEST(SemverDeepTest, FallbackVersionParsingExtensiveness)
{
    auto f1 = FallbackVersion::parse("0.0.0.1");
    ASSERT_TRUE(f1.has_value());
    EXPECT_EQ(f1->toString(), "0.0.0.1");

    auto f2 = FallbackVersion::parse("999.999.999");
    ASSERT_TRUE(f2.has_value());
    EXPECT_GT(*f2, *f1);
}

} // namespace
} // namespace linglong::package
