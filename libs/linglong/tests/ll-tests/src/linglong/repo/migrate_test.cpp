/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/migrate.h"

#include <common/tempdir.h>
#include <ostree.h>

#include <cstring>
#include <fstream>

using namespace linglong::repo;
using namespace linglong::api::types::v1;

namespace {

RepoConfigV2 makeConfig()
{
    return RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };
}

TEST(MigrateTest, NonExistentRootIsNoChange)
{
    TempDir dir;
    auto result = tryMigrate(dir.path() / "does-not-exist", makeConfig());
    EXPECT_EQ(result, MigrateResult::NoChange);
}

TEST(MigrateTest, SameVersionIsNoChange)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.14.0";
    auto result = tryMigrate(dir.path(), makeConfig());
    EXPECT_EQ(result, MigrateResult::NoChange);
}

TEST(MigrateTest, DefaultVersionWithNoRepoIsNoChange)
{
    TempDir dir;
    // No .version file present: defaults to 1.5.0, but without a repo
    // directory dispatchMigrations returns 0 (NoChange).
    auto result = tryMigrate(dir.path(), makeConfig());
    EXPECT_EQ(result, MigrateResult::NoChange);
}

TEST(MigrateTest, InvalidRepoDirectoryFails)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.5.0";
    std::filesystem::create_directories(dir.path() / "repo");
    auto result = tryMigrate(dir.path(), makeConfig());
    EXPECT_EQ(result, MigrateResult::Failed);
}

TEST(MigrateTest, UnparsableVersionFails)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "abc-version";
    auto result = tryMigrate(dir.path(), makeConfig());
    EXPECT_EQ(result, MigrateResult::Failed);
}

TEST(MigrateTest, NewerVersionWritesVersionFile)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.9.0";

    // A valid ostree repo is required: dispatchMigrations returns 0 when the
    // repo directory is missing.
    auto repoPath = dir.path() / "repo";
    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) gf = g_file_new_for_path(repoPath.c_str());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(gf);
    ASSERT_NE(repo, nullptr);
    auto created = ostree_repo_create(repo, OSTREE_REPO_MODE_BARE, nullptr, &gErr);
    ASSERT_TRUE(created) << (gErr ? gErr->message : "ostree_repo_create failed");

    auto result = tryMigrate(dir.path(), makeConfig());
    // 1.9.0 does not trigger the 1.7.0 ref migration, so dispatchMigrations
    // runs; dispatchMigrations reports a non-zero result -> Success.
    EXPECT_EQ(result, MigrateResult::Success);

    std::ifstream in{ dir.path() / ".version" };
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "1.14.0");
}

TEST(MigrateTest, RealOstreeRepoWithNoRefsIsNoChange)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.0.0";

    // Create a real (empty) ostree repository.
    auto repoPath = dir.path() / "repo";
    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) gf = g_file_new_for_path(repoPath.c_str());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(gf);
    ASSERT_NE(repo, nullptr);

    auto gv = g_variant_new("(a{sv})", nullptr);
    (void)gv;
    auto result = ostree_repo_create(repo, OSTREE_REPO_MODE_BARE, nullptr, &gErr);
    ASSERT_TRUE(result) << (gErr ? gErr->message : "ostree_repo_create failed");
    // g_autoptr(OstreeRepo) releases the repo when the test scope exits.

    auto migrate = tryMigrate(dir.path(), makeConfig());
    // Empty repo: migrateRef finds no refs and returns 0 -> NoChange.
    EXPECT_EQ(migrate, MigrateResult::NoChange);
}

} // namespace
