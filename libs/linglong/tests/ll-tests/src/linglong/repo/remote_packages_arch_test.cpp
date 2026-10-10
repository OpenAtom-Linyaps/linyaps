/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/package/reference.h"
#include "linglong/repo/remote_packages.h"

#include <string>
#include <vector>

namespace {

using namespace linglong;

namespace v1 = api::types::v1;

v1::PackageInfoV2 makePkgInfo(const std::string &module, std::vector<std::string> arch)
{
    return v1::PackageInfoV2{
        .arch = std::move(arch),
        .channel = "main",
        .id = "org.example",
        .kind = "app",
        .packageInfoV2Module = module,
        .version = "1.0.0",
    };
}

TEST(RemotePackagesArchTest, GetReferenceModulesSkipsEmptyArchEntries)
{
    // A remote index entry may contain an empty architecture array. Such an entry
    // must be skipped instead of reading arch[0] out of bounds.
    repo::RemotePackages packages;
    packages.addPackages(
      v1::Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" },
      { makePkgInfo("binary", { "x86_64" }), makePkgInfo("runtime", {}) });

    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    auto modules = packages.getReferenceModules(*ref);
    ASSERT_EQ(modules.size(), 1u);
    EXPECT_EQ(modules[0], "binary");
}

TEST(RemotePackagesArchTest, GetReferenceModulesAllEntriesEmptyArch)
{
    // Even when every entry has an empty architecture array, querying the modules
    // must not crash and simply report no match.
    repo::RemotePackages packages;
    packages.addPackages(
      v1::Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" },
      { makePkgInfo("binary", {}), makePkgInfo("runtime", {}) });

    auto ref = package::Reference::parse("main:org.example/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    auto modules = packages.getReferenceModules(*ref);
    EXPECT_TRUE(modules.empty());
}

} // namespace
