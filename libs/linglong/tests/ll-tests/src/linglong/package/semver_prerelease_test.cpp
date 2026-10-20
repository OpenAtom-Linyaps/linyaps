/*
 * SPDX-FileCopyrightText: 2025-2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

using namespace semver;

TEST(PrereleaseTest, InvalidPreReleases)
{
    EXPECT_THROW(prerelease_descriptor::parse(".alpha"), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse("alpha."), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse(".alpha."), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse("alpha. "), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse("alpha.01"), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse("+alpha.01"), semver_exception);
    EXPECT_THROW(prerelease_descriptor::parse("%alpha"), semver_exception);
}

TEST(PrereleaseTest, PreReleaseIncrement)
{
    EXPECT_EQ(prerelease_descriptor::parse("alpha-3.Beta").increment().str(), "alpha-3.Beta.0");
    EXPECT_EQ(prerelease_descriptor::parse("alpha-3.13.Beta").increment().str(), "alpha-3.14.Beta");
    EXPECT_EQ(prerelease_descriptor::parse("alpha.5.Beta.7").increment().str(), "alpha.5.Beta.8");
}

TEST(PrereleaseTest, PreReleaseEquality)
{
    EXPECT_EQ(prerelease_descriptor::parse("alpha-3.Beta.0").str(), "alpha-3.Beta.0");
    EXPECT_EQ(prerelease_descriptor::parse("alpha-3.Beta.0"),
              prerelease_descriptor::parse("alpha-3.Beta.0"));
    EXPECT_NE(prerelease_descriptor::parse("alpha-3.Beta.0"),
              prerelease_descriptor::parse("alpha-3.Beta.1"));
}

TEST(PrereleaseTest, PreReleaseDefault)
{
    EXPECT_EQ(prerelease_descriptor::initial().str(), "0");
    EXPECT_TRUE(prerelease_descriptor::empty().is_empty());
}

TEST(PrereleaseTest, NumericIdentifiersBeyondUint64ArePreserved)
{
    const std::string number = "18446744073709551616";
    EXPECT_EQ(prerelease_descriptor::parse("alpha." + number).str(), "alpha." + number);
    EXPECT_EQ(prerelease_part("42").numeric_value(), 42U);
    EXPECT_THROW(prerelease_descriptor::parse("alpha.0" + number), semver_exception);
}

TEST(PrereleaseTest, LargeNumericIdentifiersCompareNumerically)
{
    const auto smaller = prerelease_descriptor::parse("alpha.99999999999999999999");
    const auto larger = prerelease_descriptor::parse("alpha.100000000000000000000");
    EXPECT_LT(smaller.compare(larger), 0);
    EXPECT_GT(larger.compare(smaller), 0);
    EXPECT_EQ(larger.compare(larger), 0);
    EXPECT_LT(prerelease_descriptor::parse("alpha.18446744073709551616")
                .compare(prerelease_descriptor::parse("alpha.18446744073709551617")),
              0);
    EXPECT_LT(larger.compare(prerelease_descriptor::parse("alpha.beta")), 0);
}

TEST(PrereleaseTest, NumericIncrementDoesNotWrapAtUint64Boundary)
{
    EXPECT_EQ(prerelease_descriptor::parse("alpha.18446744073709551615").increment().str(),
              "alpha.18446744073709551616");
    EXPECT_EQ(prerelease_descriptor::parse("alpha.99999999999999999999.beta").increment().str(),
              "alpha.100000000000000000000.beta");
    EXPECT_EQ(prerelease_descriptor::parse("alpha.129999999999999999999").increment().str(),
              "alpha.130000000000000000000");
}

TEST(PrereleaseTest, VersionOrderingUsesLargeNumericIdentifiers)
{
    const auto smaller = version::parse("1.2.3-alpha.99999999999999999999");
    const auto larger = version::parse("1.2.3-alpha.100000000000000000000");
    EXPECT_TRUE(smaller < larger);
    EXPECT_TRUE(larger < version::parse("1.2.3"));
}
