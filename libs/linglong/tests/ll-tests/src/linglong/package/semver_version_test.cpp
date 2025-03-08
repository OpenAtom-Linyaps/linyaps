/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/package/semver.hpp"

using namespace semver;
using namespace semver::literals;

TEST(VersionTest, InvalidVersions)
{
    EXPECT_THROW(version::parse("-1.0.0"), semver_exception);
    EXPECT_THROW(version::parse("1.-1.0"), semver_exception);
    EXPECT_THROW(version::parse("0.0.-1"), semver_exception);
    EXPECT_THROW(version::parse("1"), semver_exception);
    EXPECT_THROW(version::parse(""), semver_exception);
    EXPECT_THROW(version::parse("", false), semver_exception);
    EXPECT_THROW(version::parse("1.0"), semver_exception);
    EXPECT_THROW(version::parse("1.0-alpha"), semver_exception);
    EXPECT_THROW(version::parse("1.0-alpha.01"), semver_exception);
    EXPECT_THROW(version::parse("1.0-alpha.1+security.01"), semver_exception);
    EXPECT_THROW(version::parse("1.0-alpha.1+security.0"), semver_exception);
    EXPECT_THROW(version::parse("a1.0.0"), semver_exception);
    EXPECT_THROW(version::parse("1.a0.0"), semver_exception);
    EXPECT_THROW(version::parse("1.0.a0"), semver_exception);
    EXPECT_THROW(version::parse("92233720368547758072.0.0"), semver_exception);
    EXPECT_THROW(version::parse("0.92233720368547758072.0"), semver_exception);
    EXPECT_THROW(version::parse("0.0.92233720368547758072"), semver_exception);
    EXPECT_THROW(version(1, 2, 3, ".alpha"), semver_exception);
    EXPECT_THROW(version(1, 2, 3, ".alpha."), semver_exception);
    EXPECT_THROW(version(1, 2, 3, ".alpha. "), semver_exception);
    EXPECT_THROW(version::parse("v1.0.0"), semver_exception);
    EXPECT_THROW(version::parse("v1.0-alpha.1+security.01"), semver_exception);
    EXPECT_THROW(version::parse("v1.0-alpha.1+security.0"), semver_exception);
    EXPECT_THROW(version::parse("92233720368547758072", false), semver_exception);

    EXPECT_THROW(version::parse("1"), semver_exception);
    EXPECT_THROW(version::parse("1.2"), semver_exception);
    EXPECT_THROW(version::parse("1.2.3-0123"), semver_exception);
    EXPECT_THROW(version::parse("1.2.3-0123.0123"), semver_exception);
    EXPECT_THROW(version::parse("1.1.2+.123"), semver_exception);
    EXPECT_THROW(version::parse("+invalid"), semver_exception);
    EXPECT_THROW(version::parse("-invalid"), semver_exception);
    EXPECT_THROW(version::parse("-invalid+invalid"), semver_exception);
    EXPECT_THROW(version::parse("-invalid.01"), semver_exception);
    EXPECT_THROW(version::parse("alpha"), semver_exception);
    EXPECT_THROW(version::parse("        alpha.beta"), semver_exception);
    EXPECT_THROW(version::parse("        alpha.beta.1"), semver_exception);
    EXPECT_THROW(version::parse("alpha.1"), semver_exception);
    EXPECT_THROW(version::parse("alpha+beta"), semver_exception);
    EXPECT_THROW(version::parse("        alpha_beta"), semver_exception);
    EXPECT_THROW(version::parse("alpha."), semver_exception);
    EXPECT_THROW(version::parse("alpha.."), semver_exception);
    EXPECT_THROW(version::parse("beta"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha_beta"), semver_exception);
    EXPECT_THROW(version::parse("-alpha."), semver_exception);
    EXPECT_THROW(version::parse("+security."), semver_exception);
    EXPECT_THROW(version::parse("security."), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha.."), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha..1"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha...1"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha....1"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha.....1"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha......1"), semver_exception);
    EXPECT_THROW(version::parse("1.0.0-alpha.......1"), semver_exception);
    EXPECT_THROW(version::parse("01.1.1"), semver_exception);
    EXPECT_THROW(version::parse("1.01.1"), semver_exception);
    EXPECT_THROW(version::parse("1.1.01"), semver_exception);
    EXPECT_THROW(version::parse("1.2"), semver_exception);
    EXPECT_THROW(version::parse("1.2.3.DEV"), semver_exception);
    EXPECT_THROW(version::parse("1.2-SNAPSHOT"), semver_exception);
    EXPECT_THROW(version::parse("1.2.31.2.3----RC-SNAPSHOT.12.09.1--..12+788"), semver_exception);
    EXPECT_THROW(version::parse("1.2-RC-SNAPSHOT"), semver_exception);
    EXPECT_THROW(version::parse("-1.0.3-gamma+b7718"), semver_exception);
    EXPECT_THROW(version::parse("+justmeta"), semver_exception);
    EXPECT_THROW(version::parse("9.8.7+meta+meta"), semver_exception);
    EXPECT_THROW(version::parse("9.8.7-whatever+meta+meta"), semver_exception);
    EXPECT_THROW(version::parse("99999999999999999999999.999999999999999999.99999999999999999----"
                                "RC-SNAPSHOT.12.09.1--------------------------------..12"),
                 semver_exception);
}

TEST(VersionTest, ValidVersions)
{
    EXPECT_NO_THROW(version::parse("0.0.4"));
    EXPECT_NO_THROW(version::parse("1.2.3"));
    EXPECT_NO_THROW(version::parse("10.20.30"));
    EXPECT_NO_THROW(version::parse("1.1.2-prerelease+meta"));
    EXPECT_NO_THROW(version::parse("1.1.2+meta"));
    EXPECT_NO_THROW(version::parse("1.1.2+meta-valid"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha"));
    EXPECT_NO_THROW(version::parse("1.0.0-beta"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha.beta"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha.beta.1"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha.1"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha0.valid"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha.0valid"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha-a.b-c-somethinglong+build.1-aef.1-its-okay"));
    EXPECT_NO_THROW(version::parse("1.0.0-rc.1+build.1"));
    EXPECT_NO_THROW(version::parse("2.0.0-rc.1+build.123"));
    EXPECT_NO_THROW(version::parse("1.2.3-beta"));
    EXPECT_NO_THROW(version::parse("10.2.3-DEV-SNAPSHOT"));
    EXPECT_NO_THROW(version::parse("1.2.3-SNAPSHOT-123"));
    EXPECT_NO_THROW(version::parse("1.0.0"));
    EXPECT_NO_THROW(version::parse("2.0.0"));
    EXPECT_NO_THROW(version::parse("1.1.7"));
    EXPECT_NO_THROW(version::parse("2.0.0+build.1848"));
    EXPECT_NO_THROW(version::parse("2.0.1-alpha.1227"));
    EXPECT_NO_THROW(version::parse("1.0.0-alpha+beta"));
    EXPECT_NO_THROW(version::parse("1.2.3----RC-SNAPSHOT.12.9.1--.12+788"));
    EXPECT_NO_THROW(version::parse("1.2.3----R-S.12.9.1--.12+meta"));
    EXPECT_NO_THROW(version::parse("1.2.3----RC-SNAPSHOT.12.9.1--.12"));
    EXPECT_NO_THROW(version::parse("1.0.0+0.build.1-rc.10000aaa-kk-0.1"));
    EXPECT_NO_THROW(version::parse("1.0.0-0A.is.legal"));
    EXPECT_NO_THROW(version::parse("0.0.0"));
    EXPECT_NO_THROW(version::parse("v1.0.0", false));
    EXPECT_NO_THROW(version::parse("1.0", false));
}

TEST(VersionTest, VersionStrings)
{
    EXPECT_EQ(version::parse("1.2.3").str(), "1.2.3");
    EXPECT_EQ(version::parse("1.2.3-alpha.b.3").str(), "1.2.3-alpha.b.3");
    EXPECT_EQ(version::parse("1.2.3-alpha+build").str(), "1.2.3-alpha+build");
    EXPECT_EQ(version::parse("1.2.3+build").str(), "1.2.3+build");
    EXPECT_EQ(version::parse("v1.2.3", false).str(), "1.2.3");
    EXPECT_EQ(version::parse("1.2", false).str(), "1.2.0");
    EXPECT_EQ(version::parse("v1.2", false).str(), "1.2.0");
    EXPECT_EQ(
      version::parse("18446744073709551615.18446744073709551614.18446744073709551613").str(),
      "18446744073709551615.18446744073709551614.18446744073709551613");

    EXPECT_EQ(version::parse("v1.2.3-alpha+build", false).str(), "1.2.3-alpha+build");
    EXPECT_EQ(version::parse("1.2-alpha+build", false).str(), "1.2.0-alpha+build");
    EXPECT_EQ(version::parse("v1.2-alpha+build", false).str(), "1.2.0-alpha+build");
}

TEST(VersionTest, VersionComponents)
{
    version ver = version::parse("1.2.3-alpha.b.3+build");
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_EQ(ver.prerelease(), "alpha.b.3");
    EXPECT_EQ(ver.build_meta(), "build");
    EXPECT_TRUE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionComponentsOnlyNumbers)
{
    version ver = version::parse("1.2.3");
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_TRUE(ver.is_stable());
}

TEST(VersionTest, VersionComponentsOnlyNumbersLeadingZero)
{
    version ver = version::parse("0.2.3");
    EXPECT_EQ(ver.major(), 0);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionComponentsOnlyBuildMeta)
{
    version ver = version::parse("1.2.3+build");
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_EQ(ver.build_meta(), "build");
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_TRUE(ver.is_stable());
}

TEST(VersionTest, VersionComponentsUserDefinedLiteral)
{
    version ver = "1.2.3-alpha+build"_v;
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_EQ(ver.prerelease(), "alpha");
    EXPECT_EQ(ver.build_meta(), "build");
    EXPECT_TRUE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionComponentsUserDefinedLooseLiteral)
{
    version ver = "v1.2-alpha+build"_lv;
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 0);
    EXPECT_EQ(ver.prerelease(), "alpha");
    EXPECT_EQ(ver.build_meta(), "build");
    EXPECT_TRUE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, DefaultVersion)
{
    version ver = version();
    EXPECT_EQ(ver.major(), 0);
    EXPECT_EQ(ver.minor(), 0);
    EXPECT_EQ(ver.patch(), 0);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionConstructionOnlyMajor)
{
    version ver = version(1);
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 0);
    EXPECT_EQ(ver.patch(), 0);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_TRUE(ver.is_stable());
}

TEST(VersionTest, VersionConstructionMajorMinor)
{
    version ver = version(1, 2);
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 0);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_TRUE(ver.is_stable());
}

TEST(VersionTest, VersionConstructionMajorMinorPatch)
{
    version ver = version(1, 2, 3);
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_TRUE(ver.prerelease().empty());
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_FALSE(ver.is_prerelease());
    EXPECT_TRUE(ver.is_stable());
}

TEST(VersionTest, VersionConstructionMajorMinorPatchPrerelease)
{
    version ver = version(1, 2, 3, "alpha");
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_EQ(ver.prerelease(), "alpha");
    EXPECT_TRUE(ver.build_meta().empty());
    EXPECT_TRUE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionConstructionMajorMinorPatchPrereleaseBuild)
{
    version ver = version(1, 2, 3, "alpha", "build");
    EXPECT_EQ(ver.major(), 1);
    EXPECT_EQ(ver.minor(), 2);
    EXPECT_EQ(ver.patch(), 3);
    EXPECT_EQ(ver.prerelease(), "alpha");
    EXPECT_EQ(ver.build_meta(), "build");
    EXPECT_TRUE(ver.is_prerelease());
    EXPECT_FALSE(ver.is_stable());
}

TEST(VersionTest, VersionWithoutSuffixes)
{
    version ver = version(1, 2, 3, "alpha", "build");
    version ws = ver.without_suffixes();
    EXPECT_EQ(ws.major(), 1);
    EXPECT_EQ(ws.minor(), 2);
    EXPECT_EQ(ws.patch(), 3);
    EXPECT_TRUE(ws.prerelease().empty());
    EXPECT_TRUE(ws.build_meta().empty());
    EXPECT_FALSE(ws.is_prerelease());
    EXPECT_TRUE(ws.is_stable());
}
