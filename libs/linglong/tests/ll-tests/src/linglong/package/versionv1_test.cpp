// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/versionv1.h"

using namespace linglong::package;

TEST(VersionV1, ParseRejectsOverflowedFields)
{
    const std::string tooLarge = "99999999999999999999";

    EXPECT_FALSE(VersionV1::parse(tooLarge + ".1.1").has_value());      // major
    EXPECT_FALSE(VersionV1::parse("1." + tooLarge + ".1").has_value()); // minor
    EXPECT_FALSE(VersionV1::parse("1.1." + tooLarge).has_value());      // patch
    EXPECT_FALSE(VersionV1::parse("1.1.1." + tooLarge).has_value());    // tweak
}

TEST(VersionV1, SemanticMatchHandlesMalformedAndMismatchedInput)
{
    auto v = VersionV1::parse("1.2.3");
    ASSERT_TRUE(v.has_value());

    EXPECT_FALSE(v->semanticMatch(""));      // unparsable
    EXPECT_FALSE(v->semanticMatch("1.3.3")); // minor mismatch
    EXPECT_FALSE(v->semanticMatch("1.2.4")); // patch mismatch
    EXPECT_TRUE(v->semanticMatch("1.2.3"));
}

TEST(VersionV1, SemanticMatchHandlesTweakDifference)
{
    auto v = VersionV1::parse("1.2.3.4");
    ASSERT_TRUE(v.has_value());

    EXPECT_TRUE(v->semanticMatch("1.2.3"));
    EXPECT_FALSE(v->semanticMatch("1.2.3.5"));
}

TEST(VersionV1, EqualityRejectsTweakPresenceMismatch)
{
    auto withTweak = VersionV1::parse("1.2.3.0");
    auto withoutTweak = VersionV1::parse("1.2.3");
    ASSERT_TRUE(withTweak.has_value());
    ASSERT_TRUE(withoutTweak.has_value());

    EXPECT_FALSE(*withTweak == *withoutTweak);
    EXPECT_TRUE(*withTweak != *withoutTweak);
}

TEST(VersionV1, ComparisonOperators)
{
    auto low = VersionV1::parse("1.2.3");
    auto high = VersionV1::parse("1.2.4");
    ASSERT_TRUE(low.has_value());
    ASSERT_TRUE(high.has_value());

    EXPECT_TRUE(*high > *low);
    EXPECT_FALSE(*low > *high);
    EXPECT_TRUE(*low <= *high);
    EXPECT_FALSE(*high <= *low);
    EXPECT_TRUE(*high >= *low);
    EXPECT_TRUE(*low >= *low);
}
