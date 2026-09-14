/*
 ; SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"

#include <unordered_set>

using namespace linglong::package;

TEST(Package, Reference)
{
    const std::vector<std::string> validReferences = {
        "main:com.example.App/1.0.0.0/x86_64",
        "some_channel:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0/x86_64",
        "main:com.example.App/1.0.0.0/arm64",
        "main:com.example.App/1.0.0.1/arm64",
        "main:1111/1.0.0.0/x86_64",
        "main:2222/1.0.0.0/x86_64",
        "main:3333/1.0.0.0/arm64",
        "main:4444/1.0.0.1/arm64",
    };

    for (const auto &validCase : validReferences) {
        auto refer = Reference::parse(validCase);
        ASSERT_EQ(refer.has_value(), true)
          << validCase << " is valid reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
        ASSERT_EQ(refer->toString(), validCase);
    }

    const std::vector<std::string> invalidReferences = {
        "main:com.example.App//1.0.0.0/x86_64",
        "main:1111/1.0.0.0/ x86_64",
        "main:2222/1.0.0.0/unknown",
        ":1.0.0.1-beta/arm64",
        ":com.example.App/1.0.0.0/x86_64",
    };

    for (const auto &invalidCase : invalidReferences) {
        auto refer = Reference::parse(invalidCase);
        ASSERT_EQ(refer.has_value(), false)
          << invalidCase << " is invalid reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
    }

    const std::unordered_map<std::string, std::string> validFuzzReferences = {
        { "unknown:com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/1.0.0.0/x86_64", "unknown:com.example.App/1.0.0.0/x86_64" },
        { "com.example.App/unknown/x86_64", "unknown:com.example.App/unknown/x86_64" },
        { "com.example.App/1.0.0.0/unknown", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App/1.0.0.0", "unknown:com.example.App/1.0.0.0/unknown" },
        { "com.example.App", "unknown:com.example.App/unknown/unknown" },
        { "com.example.App/1.0.0.1", "unknown:com.example.App/1.0.0.1/unknown" },
        { "3333/1.0.0.0/arm64", "unknown:3333/1.0.0.0/arm64" },
        { "4444/1.0.0.1/arm64", "unknown:4444/1.0.0.1/arm64" },
    };

    for (const auto &validCase : validFuzzReferences) {
        auto refer = FuzzyReference::parse(validCase.first);
        ASSERT_EQ(refer.has_value(), true)
          << validCase.first << " is valid fuzz reference. Error: "
          << (refer.has_value() ? "no error" : refer.error().message());
        ASSERT_EQ(refer->toString(), validCase.second);
    }
}

TEST(Package, ReferenceSemanticMatch)
{
    auto reference = Reference::parse("main:org.deepin.base/23.0.0.1/x86_64");
    ASSERT_TRUE(reference.has_value());

    const std::vector<std::pair<std::string, bool>> cases = {
        { "main:org.deepin.base/23.0.0/x86_64", true },
        { "org.deepin.base/23.0.0/x86_64", true },
        { "main:org.example.base/23.0.0/x86_64", false },
        { "stable:org.deepin.base/23.0.0/x86_64", false },
        { "main:org.deepin.base/23.0.0/arm64", false },
        { "main:org.deepin.base/24.0.0/x86_64", false },
    };

    for (const auto &[raw, expected] : cases) {
        auto fuzzy = FuzzyReference::parse(raw);
        ASSERT_TRUE(fuzzy.has_value()) << raw;
        EXPECT_EQ(reference->semanticMatch(*fuzzy), expected) << raw;
    }
}

TEST(Package, FromPackageInfoRejectsEmptyArchitecture)
{
    linglong::api::types::v1::PackageInfoV2 info{};
    info.channel = "main";
    info.id = "com.example.App";
    info.version = "1.0.0.0";
    info.arch = {};

    auto result = Reference::fromPackageInfo(info);
    ASSERT_FALSE(result.has_value())
      << "fromPackageInfo should reject empty arch array instead of crashing";
    EXPECT_NE(result.error().message().find("architecture"), std::string::npos);
}

TEST(Package, ReferenceCreate)
{
    auto ver = Version::parse("1.2.3.4");
    ASSERT_TRUE(ver.has_value());

    auto ref = Reference::create("main", "org.deepin.demo", *ver, Architecture::x86_64);
    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(ref->channel, "main");
    EXPECT_EQ(ref->id, "org.deepin.demo");
    EXPECT_EQ(ref->version, *ver);
    EXPECT_EQ(ref->arch, Architecture::x86_64);

    auto emptyChannel = Reference::create("", "org.deepin.demo", *ver, Architecture::x86_64);
    EXPECT_FALSE(emptyChannel.has_value());

    auto emptyId = Reference::create("main", "", *ver, Architecture::x86_64);
    EXPECT_FALSE(emptyId.has_value());
}

TEST(Package, ReferenceEquality)
{
    auto ref1 = Reference::parse("main:org.deepin.demo/1.0.0.0/x86_64");
    auto ref2 = Reference::parse("main:org.deepin.demo/1.0.0.0/x86_64");
    auto refDiffChannel = Reference::parse("beta:org.deepin.demo/1.0.0.0/x86_64");
    auto refDiffId = Reference::parse("main:org.deepin.other/1.0.0.0/x86_64");
    auto refDiffVersion = Reference::parse("main:org.deepin.demo/1.0.0.1/x86_64");
    auto refDiffArch = Reference::parse("main:org.deepin.demo/1.0.0.0/arm64");

    ASSERT_TRUE(ref1.has_value() && ref2.has_value() && refDiffChannel.has_value()
                && refDiffId.has_value() && refDiffVersion.has_value() && refDiffArch.has_value());

    EXPECT_EQ(*ref1, *ref2);
    EXPECT_FALSE(*ref1 != *ref2);

    EXPECT_NE(*ref1, *refDiffChannel);
    EXPECT_NE(*ref1, *refDiffId);
    EXPECT_NE(*ref1, *refDiffVersion);
    EXPECT_NE(*ref1, *refDiffArch);
}

TEST(Package, ReferenceHashAndUnorderedSet)
{
    auto ref1 = Reference::parse("main:org.deepin.demo/1.0.0.0/x86_64");
    auto ref2 = Reference::parse("main:org.deepin.demo/1.0.0.0/x86_64");
    auto ref3 = Reference::parse("main:org.deepin.demo/2.0.0.0/x86_64");
    ASSERT_TRUE(ref1.has_value() && ref2.has_value() && ref3.has_value());

    std::hash<Reference> hasher;
    EXPECT_EQ(hasher(*ref1), hasher(*ref2));

    std::unordered_set<Reference> set;
    set.insert(*ref1);
    set.insert(*ref2);
    set.insert(*ref3);
    EXPECT_EQ(set.size(), 2);
    EXPECT_NE(set.find(*ref1), set.end());
    EXPECT_NE(set.find(*ref3), set.end());
}

TEST(Package, ReferenceFromBuilderProject)
{
    // Default channel ("main") and default CPU architecture
    linglong::api::types::v1::BuilderProject defaultProject{};
    defaultProject.package.id = "org.deepin.default";
    defaultProject.package.version = "1.0.0.0";

    auto refDefault = Reference::fromBuilderProject(defaultProject);
    ASSERT_TRUE(refDefault.has_value())
      << (refDefault.has_value() ? "ok" : refDefault.error().message());
    EXPECT_EQ(refDefault->channel, "main");
    EXPECT_EQ(refDefault->id, "org.deepin.default");
    EXPECT_EQ(refDefault->version.toString(), "1.0.0.0");
    EXPECT_EQ(refDefault->arch, Architecture::currentCPUArchitecture());

    // Explicit channel and explicit architecture
    linglong::api::types::v1::BuilderProject explicitProject{};
    explicitProject.package.id = "org.deepin.custom";
    explicitProject.package.version = "2.1.0";
    explicitProject.package.channel = "beta";
    explicitProject.package.architecture = "arm64";

    auto refExplicit = Reference::fromBuilderProject(explicitProject);
    ASSERT_TRUE(refExplicit.has_value())
      << (refExplicit.has_value() ? "ok" : refExplicit.error().message());
    EXPECT_EQ(refExplicit->channel, "beta");
    EXPECT_EQ(refExplicit->id, "org.deepin.custom");
    EXPECT_EQ(refExplicit->version.toString(), "2.1.0");
    EXPECT_EQ(refExplicit->arch, Architecture::arm64);

    // Invalid version string
    linglong::api::types::v1::BuilderProject badVersionProject{};
    badVersionProject.package.id = "org.deepin.bad";
    badVersionProject.package.version = "invalid..version";

    auto refBadVersion = Reference::fromBuilderProject(badVersionProject);
    EXPECT_FALSE(refBadVersion.has_value());

    // Invalid architecture string
    linglong::api::types::v1::BuilderProject badArchProject{};
    badArchProject.package.id = "org.deepin.bad";
    badArchProject.package.version = "1.0.0.0";
    badArchProject.package.architecture = "unsupported_arch_xyz";

    auto refBadArch = Reference::fromBuilderProject(badArchProject);
    EXPECT_FALSE(refBadArch.has_value());
}

TEST(Package, FuzzyReferenceCreate)
{
    auto validFuzzy =
      FuzzyReference::create("main", "org.deepin.test", "1.0.0", Architecture::x86_64);
    ASSERT_TRUE(validFuzzy.has_value())
      << (validFuzzy.has_value() ? "ok" : validFuzzy.error().message());
    EXPECT_EQ(validFuzzy->toString(), "main:org.deepin.test/1.0.0/x86_64");

    auto partialFuzzy =
      FuzzyReference::create(std::nullopt, "org.deepin.test", std::nullopt, std::nullopt);
    ASSERT_TRUE(partialFuzzy.has_value());
    EXPECT_EQ(partialFuzzy->toString(), "unknown:org.deepin.test/unknown/unknown");

    auto emptyId = FuzzyReference::create("main", "", "1.0.0", Architecture::x86_64);
    EXPECT_FALSE(emptyId.has_value());

    auto emptyChannel =
      FuzzyReference::create(std::string(""), "org.deepin.test", "1.0.0", Architecture::x86_64);
    EXPECT_FALSE(emptyChannel.has_value());
}

TEST(Package, FuzzyReferenceParseInvalid)
{
    auto emptyInput = FuzzyReference::parse("");
    EXPECT_FALSE(emptyInput.has_value());

    auto invalidArch = FuzzyReference::parse("main:org.deepin.test/1.0.0/invalid_arch_xyz");
    EXPECT_FALSE(invalidArch.has_value());
}
