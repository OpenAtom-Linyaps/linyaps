/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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
