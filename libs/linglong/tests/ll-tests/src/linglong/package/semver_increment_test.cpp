/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

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
    EXPECT_THROW(version v1 = v.next_major("01"), semver_exception);
    EXPECT_THROW(version v2 = v.next_minor("01"), semver_exception);
    EXPECT_THROW(version v3 = v.next_patch("01"), semver_exception);
    EXPECT_THROW(version v4 = v.next_prerelease("01"), semver_exception);
    EXPECT_THROW(version v5 = v.next_security("01"), semver_exception);
}

struct VersionIncrementCase {
    std::string input_version;
    inc increment_type;
    std::string expected_version;
    std::string prerelease;
};

static const std::vector<VersionIncrementCase> kVersionIncrementCases = {
    {"1.2.3", inc::major, "2.0.0", ""},
    {"1.2.3", inc::minor, "1.3.0", ""},
    {"1.2.3", inc::patch, "1.2.4", ""},
    {"1.2.3-alpha", inc::major, "2.0.0", ""},
    {"1.2.0-0", inc::patch, "1.2.0", ""},
    {"1.2.3-4", inc::major, "2.0.0", ""},
    {"1.2.3-4", inc::minor, "1.3.0", ""},
    {"1.2.3-4", inc::patch, "1.2.3", ""},
    {"1.2.3-alpha.0.beta", inc::major, "2.0.0", ""},
    {"1.2.3-alpha.0.beta", inc::minor, "1.3.0", ""},
    {"1.2.3-alpha.0.beta", inc::patch, "1.2.3", ""},
    {"1.2.4", inc::prerelease, "1.2.5-0", ""},
    {"1.2.3-0", inc::prerelease, "1.2.3-1", ""},
    {"1.2.3-alpha.0", inc::prerelease, "1.2.3-alpha.1", ""},
    {"1.2.3-alpha.1", inc::prerelease, "1.2.3-alpha.2", ""},
    {"1.2.3-alpha.2", inc::prerelease, "1.2.3-alpha.3", ""},
    {"1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-alpha.1.beta", ""},
    {"1.2.3-alpha.1.beta", inc::prerelease, "1.2.3-alpha.2.beta", ""},
    {"1.2.3-alpha.2.beta", inc::prerelease, "1.2.3-alpha.3.beta", ""},
    {"1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-alpha.10.1.beta", ""},
    {"1.2.3-alpha.10.1.beta", inc::prerelease, "1.2.3-alpha.10.2.beta", ""},
    {"1.2.3-alpha.10.2.beta", inc::prerelease, "1.2.3-alpha.10.3.beta", ""},
    {"1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-alpha.10.beta.1", ""},
    {"1.2.3-alpha.10.beta.1", inc::prerelease, "1.2.3-alpha.10.beta.2", ""},
    {"1.2.3-alpha.10.beta.2", inc::prerelease, "1.2.3-alpha.10.beta.3", ""},
    {"1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-alpha.10.beta", ""},
    {"1.2.3-alpha.10.beta", inc::prerelease, "1.2.3-alpha.11.beta", ""},
    {"1.2.3-alpha.11.beta", inc::prerelease, "1.2.3-alpha.12.beta", ""},
    {"1.2.0", inc::patch, "1.2.1", ""},
    {"1.2.0-1", inc::patch, "1.2.0", ""},
    {"1.2.0", inc::minor, "1.3.0", ""},
    {"1.2.3-1", inc::minor, "1.3.0", ""},
    {"1.2.0", inc::major, "2.0.0", ""},
    {"1.2.3-1", inc::major, "2.0.0", ""},
    {"1.2.4", inc::prerelease, "1.2.5-dev", "dev"},
    {"1.2.3-0", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.0", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.0", inc::prerelease, "1.2.3-alpha.1", "alpha"},
    {"1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.0.beta", inc::prerelease, "1.2.3-alpha.1.beta", "alpha"},
    {"1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.10.0.beta", inc::prerelease, "1.2.3-alpha.10.1.beta", "alpha"},
    {"1.2.3-alpha.10.1.beta", inc::prerelease, "1.2.3-alpha.10.2.beta", "alpha"},
    {"1.2.3-alpha.10.2.beta", inc::prerelease, "1.2.3-alpha.10.3.beta", "alpha"},
    {"1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.10.beta.0", inc::prerelease, "1.2.3-alpha.10.beta.1", "alpha"},
    {"1.2.3-alpha.10.beta.1", inc::prerelease, "1.2.3-alpha.10.beta.2", "alpha"},
    {"1.2.3-alpha.10.beta.2", inc::prerelease, "1.2.3-alpha.10.beta.3", "alpha"},
    {"1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-dev", "dev"},
    {"1.2.3-alpha.9.beta", inc::prerelease, "1.2.3-alpha.10.beta", "alpha"},
    {"1.2.3-alpha.10.beta", inc::prerelease, "1.2.3-alpha.11.beta", "alpha"},
    {"1.2.3-alpha.11.beta", inc::prerelease, "1.2.3-alpha.12.beta", "alpha"},
    {"1.2.0", inc::patch, "1.2.1-dev", "dev"},
    {"1.2.0-1", inc::patch, "1.2.1-dev", "dev"},
    {"1.2.0", inc::minor, "1.3.0-dev", "dev"},
    {"1.2.3-1", inc::minor, "1.3.0-dev", "dev"},
    {"1.2.0", inc::major, "2.0.0-dev", "dev"},
    {"1.2.3-1", inc::major, "2.0.0-dev", "dev"},
    {"1.2.0-1", inc::minor, "1.3.0", ""},
    {"1.0.0-1", inc::major, "2.0.0", ""},
    {"1.2.3-dev.beta", inc::prerelease, "1.2.3-dev.beta.0", "dev"},
    {"1.2.3+security.1", inc::major, "2.0.0", ""},
    {"1.2.3+security.1", inc::minor, "1.3.0", ""},
    {"1.2.3+security.1", inc::patch, "1.2.4", ""},
    {"1.2.3", inc::security, "1.2.3+security.1", ""},
    {"1.2.3+security.1", inc::security, "1.2.3+security.2", ""},
    {"1.2.3+security.9", inc::security, "1.2.3+security.10", ""},
    {"1.2.3-alpha.0", inc::security, "1.2.3-alpha.0+security.1", ""},
    {"1.2.3-alpha.1", inc::security, "1.2.3-alpha.1+security.1", ""},
    {"1.2.3-alpha.1+security.2", inc::security, "1.2.3-alpha.1+security.3", ""},
    {"1.2.3+build.5", inc::security, "1.2.3+build.5.security.1", ""},
    {"1.2.3+build.5.security.1", inc::security, "1.2.3+build.5.security.2", ""}
};

class VersionIncrementTest : public ::testing::Test
{
protected:
    void RunCase(const VersionIncrementCase& c) {
        version v = version::parse(c.input_version);
        EXPECT_EQ(c.expected_version, v.increment(c.increment_type, c.prerelease).str()) 
            << "Input: " << c.input_version
            << ", Increment: " << static_cast<int>(c.increment_type)
            << ", Prerelease: " << c.prerelease;
    }
};

TEST(VersionIncrementTest, IncrementationTable) {
    for (const auto& c : kVersionIncrementCases) {
        SCOPED_TRACE("Input: " + c.input_version +
                     ", IncType: " + std::to_string(static_cast<int>(c.increment_type)) +
                     ", Pre: " + c.prerelease);
        version v = version::parse(c.input_version);
        EXPECT_EQ(c.expected_version, v.increment(c.increment_type, c.prerelease).str())
            << "Input: " << c.input_version
            << ", Increment: " << static_cast<int>(c.increment_type)
            << ", Prerelease: " << c.prerelease;
    }
}