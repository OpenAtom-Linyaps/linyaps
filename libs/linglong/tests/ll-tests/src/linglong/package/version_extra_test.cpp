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

using namespace linglong::package;

TEST(FallbackVersionCrossType, CompareWithVersionV1)
{
    auto fv = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(fv.has_value());
    auto v1 = VersionV1::parse("1.2.3");
    ASSERT_TRUE(v1.has_value());
    auto v1Higher = VersionV1::parse("1.2.4");
    ASSERT_TRUE(v1Higher.has_value());

    EXPECT_TRUE(*fv == *v1);
    EXPECT_FALSE(*fv != *v1);
    EXPECT_TRUE(*fv <= *v1);
    EXPECT_TRUE(*fv >= *v1);

    EXPECT_TRUE(*fv < *v1Higher);
    EXPECT_TRUE(*fv <= *v1Higher);
    EXPECT_TRUE(*v1Higher > *fv);
    EXPECT_TRUE(*v1Higher >= *fv);
    EXPECT_TRUE(*fv != *v1Higher);
}

TEST(FallbackVersionCrossType, CompareWithVersionV2)
{
    auto fv = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(fv.has_value());
    auto v2 = VersionV2::parse("1.2.3");
    ASSERT_TRUE(v2.has_value());
    auto v2Lower = VersionV2::parse("1.2.2");
    ASSERT_TRUE(v2Lower.has_value());

    EXPECT_TRUE(*fv == *v2);
    EXPECT_FALSE(*fv != *v2);
    EXPECT_TRUE(*fv >= *v2);
    EXPECT_TRUE(*fv <= *v2);

    EXPECT_TRUE(*fv > *v2Lower);
    EXPECT_TRUE(*fv >= *v2Lower);
    EXPECT_TRUE(*v2Lower < *fv);
    EXPECT_TRUE(*fv != *v2Lower);
}

TEST(FallbackVersionTest, SemanticMatch)
{
    auto fv = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(fv.has_value());
    EXPECT_TRUE(fv->semanticMatch("1.2.3"));
    EXPECT_TRUE(fv->semanticMatch("1.2"));
    EXPECT_FALSE(fv->semanticMatch("1.2.3.4"));
    EXPECT_FALSE(fv->semanticMatch("1.2.4"));
    EXPECT_FALSE(fv->semanticMatch("2.2.3"));
}

TEST(FallbackVersionTest, ToString)
{
    auto fv = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(fv.has_value());
    EXPECT_EQ(fv->toString(), "1.2.3");
}

TEST(FallbackVersionTest, CompareWithOtherVersion)
{
    auto fv = FallbackVersion::parse("1.2.3");
    ASSERT_TRUE(fv.has_value());
    EXPECT_LT(fv->compareWithOtherVersion("1.2.10"), 0);
    EXPECT_GT(fv->compareWithOtherVersion("1.2.2"), 0);
    EXPECT_EQ(fv->compareWithOtherVersion("1.2.3"), 0);
    // unparsable raw version -> 0
    EXPECT_EQ(fv->compareWithOtherVersion(""), 0);
}

TEST(VersionTest, ValidateDependVersion)
{
    // The regex requires MAJOR.MINOR[.PATCH]
    EXPECT_FALSE(Version::validateDependVersion("1").has_value());
    EXPECT_TRUE(Version::validateDependVersion("1.2").has_value());
    EXPECT_TRUE(Version::validateDependVersion("1.2.3").has_value());
    EXPECT_FALSE(Version::validateDependVersion("1.2.3.4").has_value());
    EXPECT_FALSE(Version::validateDependVersion("1.2.3-alpha").has_value());
    EXPECT_FALSE(Version::validateDependVersion("").has_value());
}

TEST(VersionTest, ParseFallbackAndToString)
{
    auto version = Version::parse("1.2.3.4.5");
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->toString(), "1.2.3.4.5");

    auto semver = Version::parse("1.2.3");
    ASSERT_TRUE(semver.has_value());
    EXPECT_EQ(semver->toString(), "1.2.3");
}

TEST(VersionTest, SemanticMatchOutsideFallback)
{
    auto v2 = Version::parse("1.2.3-alpha");
    ASSERT_TRUE(v2.has_value());
    EXPECT_TRUE(v2->semanticMatch("1.2.3"));
    EXPECT_FALSE(v2->semanticMatch("1.2.4"));
}

TEST(VersionTest, IgnoreTweakAndVersionV1Checks)
{
    auto v1 = Version::parse("1.2.3.4");
    ASSERT_TRUE(v1.has_value());
    EXPECT_TRUE(v1->isVersionV1());
    EXPECT_TRUE(v1->hasTweak());
    v1->ignoreTweak();
    EXPECT_FALSE(v1->hasTweak());

    auto v2 = Version::parse("1.2.3");
    ASSERT_TRUE(v2.has_value());
    EXPECT_FALSE(v2->isVersionV1());
    EXPECT_FALSE(v2->hasTweak());
}

TEST(VersionTest, FilterByFuzzyVersion)
{
    using linglong::api::types::v1::PackageInfoV2;

    PackageInfoV2 matching;
    matching.id = "org.deepin.demo";
    matching.version = "1.2.3";
    matching.schemaVersion = "1.0";
    matching.kind = "app";
    matching.size = 0;

    PackageInfoV2 other;
    other.id = "org.deepin.other";
    other.version = "2.0.0";
    other.schemaVersion = "1.0";
    other.kind = "app";
    other.size = 0;

    auto list = Version::filterByFuzzyVersion(std::vector<PackageInfoV2>{ matching, other }, "1.2");
    ASSERT_EQ(list.size(), 1);
    EXPECT_EQ(list[0].id, "org.deepin.demo");

    // No matches -> empty result
    auto empty = Version::filterByFuzzyVersion(std::vector<PackageInfoV2>{ matching }, "9.9");
    EXPECT_TRUE(empty.empty());
}
