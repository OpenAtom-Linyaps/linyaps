/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/package/reference.h"
#include "linglong/repo/remote_packages.h"

#include <string>
#include <vector>

namespace {

using namespace linglong;
using ::testing::ElementsAre;

namespace v1 = api::types::v1;

v1::PackageInfoV2 makePkgInfo(const std::string &id,
                              const std::string &version,
                              const std::string &module = "binary")
{
    return v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = id,
        .kind = "app",
        .packageInfoV2Module = module,
        .version = version,
    };
}

TEST(RemotePackagesTest, GetLatestPackageWhenEmptyReturnsError)
{
    repo::RemotePackages packages;

    auto result = packages.getLatestPackage();

    EXPECT_FALSE(result.has_value());
}

TEST(RemotePackagesTest, GetLatestPackageSingleRepoReturnsHighestVersion)
{
    repo::RemotePackages packages;
    packages.addPackages(
      v1::Repo{ .name = "stable" },
      { makePkgInfo("org.example", "1.0.0"), makePkgInfo("org.example", "2.1.0") });

    auto result = packages.getLatestPackage();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first.get().name, "stable");
    EXPECT_EQ(result->second.get().version, "2.1.0");
}

TEST(RemotePackagesTest, GetLatestPackagePrefersParsableVersion)
{
    // An empty version is not parseable by any of the version formats, so the
    // comparator must prefer the parseable record over it.
    repo::RemotePackages packages;
    packages.addPackages(v1::Repo{ .name = "stable" },
                         { makePkgInfo("org.example", "1.0.0"), makePkgInfo("org.example", "") });

    auto result = packages.getLatestPackage();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->second.get().version, "1.0.0");
}

TEST(RemotePackagesTest, GetLatestPackageAcrossMultipleReposPicksGlobalMax)
{
    repo::RemotePackages packages;
    packages.addPackages(v1::Repo{ .name = "stable" }, { makePkgInfo("org.example", "1.0.0") });
    packages.addPackages(v1::Repo{ .name = "edge" }, { makePkgInfo("org.example", "3.0.0") });

    auto result = packages.getLatestPackage();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first.get().name, "edge");
    EXPECT_EQ(result->second.get().version, "3.0.0");
}

TEST(RemotePackagesTest, GetReferenceModulesReturnsMatchingModules)
{
    repo::RemotePackages packages;
    packages.addPackages(v1::Repo{ .name = "stable" },
                         { makePkgInfo("org.example", "1.0.0", "binary"),
                           makePkgInfo("org.example", "1.0.0", "develop"),
                           makePkgInfo("org.example", "2.0.0", "binary"),
                           makePkgInfo("other.app", "1.0.0", "binary") });

    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    auto modules = packages.getReferenceModules(*ref);
    ASSERT_EQ(modules.size(), 2u);
    EXPECT_THAT(modules, ElementsAre("binary", "develop"));
}

TEST(RemotePackagesTest, GetReferenceModulesReturnsEmptyWhenNoMatch)
{
    repo::RemotePackages packages;
    packages.addPackages(v1::Repo{ .name = "stable" }, { makePkgInfo("org.example", "9.9.9") });

    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_TRUE(packages.getReferenceModules(*ref).empty());
}

} // namespace
