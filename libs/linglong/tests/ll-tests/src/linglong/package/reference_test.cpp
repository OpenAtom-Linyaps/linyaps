/*
 ; SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/api/types/v1/BuilderProject.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/package/architecture.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/package/version.h"

using namespace linglong::package;

#include <unordered_map>

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

TEST(Package, ReferenceCreateRejectsEmptyChannelAndId)
{
    const auto version = Version::parse("1.0.0");
    ASSERT_TRUE(version.has_value());
    const auto arch = Architecture::parse("x86_64");
    ASSERT_TRUE(arch.has_value());

    auto noChannel = Reference::create("", "com.example.App", *version, *arch);
    ASSERT_FALSE(noChannel.has_value());

    auto noId = Reference::create("main", "", *version, *arch);
    ASSERT_FALSE(noId.has_value());
}

TEST(Package, FromBuilderProjectUsesDefaults)
{
    linglong::api::types::v1::BuilderProject project;
    project.package.version = "1.0.0";
    project.package.id = "com.example.App";

    auto reference = Reference::fromBuilderProject(project);
    ASSERT_TRUE(reference.has_value()) << reference.error().message();

    EXPECT_EQ(reference->channel, "main");
    EXPECT_EQ(reference->id, "com.example.App");
    EXPECT_EQ(reference->toString(),
              "main:com.example.App/1.0.0/" + Architecture::currentCPUArchitecture().toString());
}

TEST(Package, FromBuilderProjectUsesExplicitArchitectureAndChannel)
{
    linglong::api::types::v1::BuilderProject project;
    project.package.version = "1.0.0";
    project.package.id = "com.example.App";
    project.package.architecture = "arm64";
    project.package.channel = "stable";

    auto reference = Reference::fromBuilderProject(project);
    ASSERT_TRUE(reference.has_value()) << reference.error().message();

    EXPECT_EQ(reference->channel, "stable");
    EXPECT_EQ(reference->arch.toString(), "arm64");
    EXPECT_EQ(reference->toString(), "stable:com.example.App/1.0.0/arm64");
}

TEST(Package, FromBuilderProjectRejectsInvalidVersion)
{
    linglong::api::types::v1::BuilderProject project;
    project.package.version = "...";
    project.package.id = "com.example.App";

    auto reference = Reference::fromBuilderProject(project);
    ASSERT_FALSE(reference.has_value());
}

TEST(Package, FromBuilderProjectRejectsInvalidArchitecture)
{
    linglong::api::types::v1::BuilderProject project;
    project.package.version = "1.0.0";
    project.package.id = "com.example.App";
    project.package.architecture = "unknown_arch";

    auto reference = Reference::fromBuilderProject(project);
    ASSERT_FALSE(reference.has_value());
    EXPECT_TRUE(reference.error().message().find("unknown architecture") != std::string::npos);
}

TEST(Package, FromPackageInfoRejectsInvalidVersion)
{
    linglong::api::types::v1::PackageInfoV2 info{};
    info.channel = "main";
    info.id = "com.example.App";
    info.version = "...";
    info.arch = { "x86_64" };

    auto result = Reference::fromPackageInfo(info);
    ASSERT_FALSE(result.has_value());
}

TEST(Package, FromPackageInfoRejectsInvalidArchitectureString)
{
    linglong::api::types::v1::PackageInfoV2 info{};
    info.channel = "main";
    info.id = "com.example.App";
    info.version = "1.0.0";
    info.arch = { "not-an-arch" };

    auto result = Reference::fromPackageInfo(info);
    ASSERT_FALSE(result.has_value());
}

TEST(Package, ReferenceHashWorksInUnorderedMap)
{
    const auto reference = Reference::parse("main:com.example.App/1.0.0.0/x86_64");
    ASSERT_TRUE(reference.has_value());

    const auto other = Reference::parse("main:com.example.App/1.0.0.0/arm64");
    ASSERT_TRUE(other.has_value());

    std::unordered_map<Reference, int> refs;
    refs.emplace(*reference, 10);
    refs.emplace(*other, 20);

    EXPECT_EQ(refs.size(), 2u);
    auto it = refs.find(*reference);
    ASSERT_NE(it, refs.end());
    EXPECT_EQ(it->second, 10);
    EXPECT_EQ(refs.at(*other), 20);
}
