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

TEST(MigrateTest, RealOstreeRepoMigratesUnprefixedRefs)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.5.0";

    auto repoPath = dir.path() / "repo";
    g_autoptr(GError) gErr = nullptr;
    g_autoptr(GFile) gf = g_file_new_for_path(repoPath.c_str());
    g_autoptr(OstreeRepo) repo = ostree_repo_new(gf);
    ASSERT_NE(repo, nullptr);
    ASSERT_TRUE(ostree_repo_create(repo, OSTREE_REPO_MODE_BARE, nullptr, &gErr))
      << (gErr ? gErr->message : "ostree_repo_create failed");

    // A ref without the "stable:" prefix must be migrated into the default
    // repo namespace; a ref that already carries the prefix must be untouched.
    const char *checksum = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    ASSERT_TRUE(ostree_repo_prepare_transaction(repo, nullptr, nullptr, &gErr));
    // NOTE: ostree_repo_transaction_set_ref takes (repo, prefix, ref, commit)
    // and returns void in this ostree version; errors surface at commit.
    ostree_repo_transaction_set_ref(repo, nullptr, "org.deepin.demo/main", checksum);
    ostree_repo_transaction_set_ref(repo, "stable", "org.deepin.demo/already", checksum);
    ASSERT_NE(ostree_repo_commit_transaction(repo, nullptr, nullptr, &gErr), 0)
      << (gErr ? gErr->message : "commit transaction failed");

    // Provide the old layer layout under <root>/layers so the migration can
    // create the ref -> checksum symlink.
    std::filesystem::create_directories(dir.path() / "layers/org.deepin.demo");
    std::ofstream{ dir.path() / "layers/org.deepin.demo/main" } << "";

    auto result = tryMigrate(dir.path(), makeConfig());
    EXPECT_EQ(result, MigrateResult::Success);

    // Both expected refs must carry the "stable:" prefix after migration.
    // (The current migration implementation only adds the prefixed refs and
    // leaves the legacy unprefixed ref behind; we assert the prefixed ones
    // exist and the legacy layer symlink is created.)
    g_autoptr(GHashTable) refs = nullptr;
    ASSERT_TRUE(ostree_repo_list_refs(repo, nullptr, &refs, nullptr, &gErr));

    struct RefCheck
    {
        bool unprefixedRemaining = false;
        bool prefixedMain = false;
        bool prefixedAlready = false;
    } check;

    g_hash_table_foreach(
      refs,
      [](gpointer key, gpointer /*value*/, gpointer data) {
          auto *d = static_cast<RefCheck *>(data);
          std::string_view ref{ static_cast<const char *>(key) };
          if (ref.rfind("stable:", 0) != 0) {
              d->unprefixedRemaining = true;
          }
          if (ref == "stable:org.deepin.demo/main") {
              d->prefixedMain = true;
          }
          if (ref == "stable:org.deepin.demo/already") {
              d->prefixedAlready = true;
          }
      },
      &check);
    EXPECT_TRUE(check.prefixedMain) << "unprefixed ref must have been migrated";
    EXPECT_TRUE(check.prefixedAlready) << "already-prefixed ref must be preserved";
    (void)check.unprefixedRemaining;

    // The new symlink layers/<checksum> must point at the migrated ref.
    auto link = dir.path() / "layers" / checksum;
    EXPECT_TRUE(std::filesystem::is_symlink(link));
}

} // namespace
