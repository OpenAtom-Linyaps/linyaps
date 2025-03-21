/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

#include <tuple>
#include <vector>

using namespace semver;

TEST(VersionIncrementTest, BasicIncrementation)
{
    version v = version::parse("1.2.3-alpha.4+build.3");
    EXPECT_EQ("2.0.0", v.next_major().str());
    EXPECT_EQ("1.3.0", v.next_minor().str());
    EXPECT_EQ("1.2.3", v.next_patch().str());
    EXPECT_EQ("1.2.3-alpha.5", v.next_prerelease().str());
    EXPECT_EQ("1.2.3-alpha.4+build.3.security.1", v.next_security().str());
}

TEST(VersionIncrementTest, IncrementationWithoutPreRelease)
{
    version v = version::parse("1.2.3");
    EXPECT_EQ("2.0.0", v.next_major().str());
    EXPECT_EQ("1.3.0", v.next_minor().str());
    EXPECT_EQ("1.2.4", v.next_patch().str());
    EXPECT_EQ("1.2.4-0", v.next_prerelease().str());
    EXPECT_EQ("1.2.3+security.1", v.next_security().str());
}

TEST(VersionIncrementTest, IncrementationWithoutNumericPreRelease)
{
    version v = version::parse("1.2.3-alpha");
    EXPECT_EQ("2.0.0", v.next_major().str());
    EXPECT_EQ("1.3.0", v.next_minor().str());
    EXPECT_EQ("1.2.3", v.next_patch().str());
    EXPECT_EQ("1.2.3-alpha.0", v.next_prerelease().str());
    EXPECT_EQ("1.2.3-alpha+security.1", v.next_security().str());
}

TEST(VersionIncrementTest, IncrementationWithoutNumericPreReleaseWithIncrement)
{
    version v = version::parse("1.2.3-alpha");
    EXPECT_EQ("2.0.0", v.increment(inc::major).str());
    EXPECT_EQ("1.3.0", v.increment(inc::minor).str());
    EXPECT_EQ("1.2.3", v.increment(inc::patch).str());
    EXPECT_EQ("1.2.3-alpha.0", v.increment(inc::prerelease).str());
    EXPECT_EQ("1.2.3-alpha+security.1", v.increment(inc::security).str());
}

TEST(VersionIncrementTest, IncrementationWithInvalidPreRelease)
{
    version v = version::parse("1.2.3-alpha");
    EXPECT_THROW(v.next_major("01"), semver_exception);
    EXPECT_THROW(v.next_minor("01"), semver_exception);
    EXPECT_THROW(v.next_patch("01"), semver_exception);
    EXPECT_THROW(v.next_prerelease("01"), semver_exception);
    EXPECT_THROW(v.next_security("01"), semver_exception);
}

class VersionIncrementParameterizedTest
    : public ::testing::TestWithParam<std::tuple<std::string, inc, std::string, std::string>>
{
};

TEST_P(VersionIncrementParameterizedTest, IncrementationTable)
{
    auto [input_version, increment_type, expected_version, prerelease] = GetParam();
    version v = version::parse(input_version);
    EXPECT_EQ(expected_version, v.increment(increment_type, prerelease).str());
}

INSTANTIATE_TEST_SUITE_P(
  VersionIncrementTests,
  VersionIncrementParameterizedTest,
  ::testing::Values(
    std::make_tuple("1.2.3", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.3", inc::patch, "1.2.4", ""),
    std::make_tuple("1.2.3-alpha", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.0-0", inc::patch, "1.2.0", ""),
    std::make_tuple("1.2.3-4", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3-4", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.3-4", inc::patch, "1.2.3", ""),
    std::make_tuple("1.2.3-alpha.0.beta", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3-alpha.0.beta", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.3-alpha.0.beta", inc::patch, "1.2.3", ""),
    std::make_tuple("1.2.4", inc::prerelease, "1.2.5-0", ""),
    std::make_tuple("1.2.3-0", inc::prerelease, "1.2.3-1", ""),
    std::make_tuple("1.2.3-alpha.0", inc::prerelease, "1.2.3-alpha.1", ""),
    std::make_tuple("1.2.3-alpha.1", inc::prerelease, "1.2.3-alpha.2", ""),
    std::make_tuple("1.2.3-alpha.2", inc::prerelease, "1.2.3-alpha.3", ""),
    std::make_tuple("1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-alpha.1.beta", ""),
    std::make_tuple("1.2.3-alpha.1.beta", inc::prerelease, "1.2.3-alpha.2.beta", ""),
    std::make_tuple("1.2.3-alpha.2.beta", inc::prerelease, "1.2.3-alpha.3.beta", ""),
    std::make_tuple("1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-alpha.10.1.beta", ""),
    std::make_tuple("1.2.3-alpha.10.1.beta", inc::prerelease, "1.2.3-alpha.10.2.beta", ""),
    std::make_tuple("1.2.3-alpha.10.2.beta", inc::prerelease, "1.2.3-alpha.10.3.beta", ""),
    std::make_tuple("1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-alpha.10.beta.1", ""),
    std::make_tuple("1.2.3-alpha.10.beta.1", inc::prerelease, "1.2.3-alpha.10.beta.2", ""),
    std::make_tuple("1.2.3-alpha.10.beta.2", inc::prerelease, "1.2.3-alpha.10.beta.3", ""),
    std::make_tuple("1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-alpha.10.beta", ""),
    std::make_tuple("1.2.3-alpha.10.beta", inc::prerelease, "1.2.3-alpha.11.beta", ""),
    std::make_tuple("1.2.3-alpha.11.beta", inc::prerelease, "1.2.3-alpha.12.beta", ""),
    std::make_tuple("1.2.0", inc::patch, "1.2.1", ""),
    std::make_tuple("1.2.0-1", inc::patch, "1.2.0", ""),
    std::make_tuple("1.2.0", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.3-1", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.0", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3-1", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.4", inc::prerelease, "1.2.5-dev", "dev"),
    std::make_tuple("1.2.3-0", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.0", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.0", inc::prerelease, "1.2.3-alpha.1", "alpha"),
    std::make_tuple("1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-alpha.1.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-alpha.10.1.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.10.1.beta", inc::prerelease, "1.2.3-alpha.10.2.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.10.2.beta", inc::prerelease, "1.2.3-alpha.10.3.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-alpha.10.beta.1", "alpha"),
    std::make_tuple("1.2.3-alpha.10.beta.1", inc::prerelease, "1.2.3-alpha.10.beta.2", "alpha"),
    std::make_tuple("1.2.3-alpha.10.beta.2", inc::prerelease, "1.2.3-alpha.10.beta.3", "alpha"),
    std::make_tuple("1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-dev", "dev"),
    std::make_tuple("1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-alpha.10.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.10.beta", inc::prerelease, "1.2.3-alpha.11.beta", "alpha"),
    std::make_tuple("1.2.3-alpha.11.beta", inc::prerelease, "1.2.3-alpha.12.beta", "alpha"),
    std::make_tuple("1.2.0", inc::patch, "1.2.1-dev", "dev"),
    std::make_tuple("1.2.0-1", inc::patch, "1.2.1-dev", "dev"),
    std::make_tuple("1.2.0", inc::minor, "1.3.0-dev", "dev"),
    std::make_tuple("1.2.3-1", inc::minor, "1.3.0-dev", "dev"),
    std::make_tuple("1.2.0", inc::major, "2.0.0-dev", "dev"),
    std::make_tuple("1.2.3-1", inc::major, "2.0.0-dev", "dev"),
    std::make_tuple("1.2.0-1", inc::minor, "1.3.0", ""),
    std::make_tuple("1.0.0-1", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3-dev.beta", inc::prerelease, "1.2.3-dev.beta.0", "dev"),
    std::make_tuple("1.2.3+security.1", inc::major, "2.0.0", ""),
    std::make_tuple("1.2.3+security.1", inc::minor, "1.3.0", ""),
    std::make_tuple("1.2.3+security.1", inc::patch, "1.2.4", ""),
    std::make_tuple("1.2.3", inc::security, "1.2.3+security.1", ""),
    std::make_tuple("1.2.3+security.1", inc::security, "1.2.3+security.2", ""),
    std::make_tuple("1.2.3+security.9", inc::security, "1.2.3+security.10", ""),
    std::make_tuple("1.2.3-alpha.0", inc::security, "1.2.3-alpha.0+security.1", ""),
    std::make_tuple("1.2.3-alpha.1", inc::security, "1.2.3-alpha.1+security.1", ""),
    std::make_tuple("1.2.3-alpha.1+security.2", inc::security, "1.2.3-alpha.1+security.3", ""),
    std::make_tuple("1.2.3+build.5", inc::security, "1.2.3+build.5.security.1", ""),
    std::make_tuple("1.2.3+build.5.security.1", inc::security, "1.2.3+build.5.security.2", "")));
