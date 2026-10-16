/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "configure.h"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/migrate.h"

#include <common/tempdir.h>
#include <ostree.h>

#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

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

// A single ref to seed into a test ostree repository. A null prefix selects
// the legacy, unprefixed ref namespace written by repositories created before
// the 1.7.0 migration.
struct RefEntry
{
    const char *prefix;
    const char *ref;
    const char *checksum;
};

// Create a bare ostree repository at repoPath and return an owned handle. The
// caller stores the result in a g_autoptr so the handle is released when the
// test ends.
OstreeRepo *createBareRepo(const std::filesystem::path &repoPath, GError **err)
{
    g_autoptr(GFile) gf = g_file_new_for_path(repoPath.c_str());
    OstreeRepo *repo = ostree_repo_new(gf);
    if (repo == nullptr) {
        return nullptr;
    }

    if (ostree_repo_create(repo, OSTREE_REPO_MODE_BARE, nullptr, err) == FALSE) {
        g_object_unref(repo);
        return nullptr;
    }

    return repo;
}

// Commit a batch of refs atomically so the on-disk ref table holds both the
// legacy names and the already-prefixed names before the migration runs.
bool writeRefs(OstreeRepo *repo, const std::vector<RefEntry> &entries, GError **err)
{
    if (ostree_repo_prepare_transaction(repo, nullptr, nullptr, err) == FALSE) {
        return false;
    }

    for (const auto &entry : entries) {
        ostree_repo_transaction_set_ref(repo, entry.prefix, entry.ref, entry.checksum);
    }

    return ostree_repo_commit_transaction(repo, nullptr, nullptr, err) != 0;
}

// Read the whole ref table into a name to commit map so tests can assert on the
// exact commit a ref points at, not merely on the ref being present.
std::map<std::string, std::string> readRefs(OstreeRepo *repo, GError **err)
{
    g_autoptr(GHashTable) refs = nullptr;
    std::map<std::string, std::string> result;
    if (ostree_repo_list_refs(repo, nullptr, &refs, nullptr, err) == FALSE) {
        return result;
    }

    g_hash_table_foreach(
      refs,
      [](gpointer key, gpointer value, gpointer data) {
          auto &out = *static_cast<std::map<std::string, std::string> *>(data);
          out.emplace(static_cast<const char *>(key), static_cast<const char *>(value));
      },
      &result);

    return result;
}

// Create the legacy <root>/layers/<ref> path so the migration can create the
// ref to checksum symlink exactly as it would on a real installation.
void createLegacyLayer(const std::filesystem::path &root, const std::string &ref)
{
    auto layerPath = root / "layers" / ref;
    std::filesystem::create_directories(layerPath.parent_path());
    std::ofstream{ layerPath } << "";
}

// Distinct, syntactically valid 64-character hex commit ids. The migration
// copies ref values without validating the commit, but every ref in a test
// uses its own id so an overwritten ref is detectable.
constexpr const char *kChecksumLegacyA =
  "1111111111111111111111111111111111111111111111111111111111111111";
constexpr const char *kChecksumLegacyB =
  "2222222222222222222222222222222222222222222222222222222222222222";
constexpr const char *kChecksumLegacyC =
  "3333333333333333333333333333333333333333333333333333333333333333";
constexpr const char *kChecksumKeptA =
  "4444444444444444444444444444444444444444444444444444444444444444";
constexpr const char *kChecksumKeptB =
  "5555555555555555555555555555555555555555555555555555555555555555";
constexpr const char *kChecksumKeptC =
  "6666666666666666666666666666666666666666666666666666666666666666";

constexpr const char *kLegacyRefA = "com.example.a/1.0.0/x86_64/main";
constexpr const char *kLegacyRefB = "com.example.b/1.0.0/x86_64/main";
constexpr const char *kLegacyRefC = "com.example.c/1.0.0/x86_64/main";

TEST(MigrateTest, NonExistentRootIsNoChange)
{
    TempDir dir;
    auto result = tryMigrate(dir.path() / "does-not-exist", makeConfig());
    EXPECT_EQ(result, MigrateResult::NoChange);
}

TEST(MigrateTest, SameVersionIsNoChange)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << LINGLONG_VERSION;
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
    EXPECT_EQ(content, LINGLONG_VERSION);
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

TEST(MigrateTest, LegacyRefsWithExistingPrefixedCounterpartsAreNotMigrated)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.5.0";

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(OstreeRepo) repo = createBareRepo(dir.path() / "repo", &gErr);
    ASSERT_NE(repo, nullptr) << (gErr ? gErr->message : "ostree_repo_create failed");

    // Every legacy unprefixed ref already has a prefixed counterpart holding a
    // different commit. The migration must notice this and leave all of them
    // alone. The previous growing-prefix lookup re-migrated all but the first
    // ref and overwrote the commits that were already present.
    ASSERT_TRUE(writeRefs(repo,
                          {
                            { nullptr, kLegacyRefA, kChecksumLegacyA },
                            { nullptr, kLegacyRefB, kChecksumLegacyB },
                            { nullptr, kLegacyRefC, kChecksumLegacyC },
                            { "stable", kLegacyRefA, kChecksumKeptA },
                            { "stable", kLegacyRefB, kChecksumKeptB },
                            { "stable", kLegacyRefC, kChecksumKeptC },
                          },
                          &gErr))
      << (gErr ? gErr->message : "writing refs failed");

    // No prefixed counterpart is missing, so migrateRef reports no change and
    // must not prepare a transaction at all.
    EXPECT_EQ(tryMigrate(dir.path(), makeConfig()), MigrateResult::NoChange);

    auto refs = readRefs(repo, &gErr);
    ASSERT_FALSE(refs.empty()) << (gErr ? gErr->message : "listing refs failed");

    // The already-migrated refs must still point at their original commits.
    EXPECT_EQ(refs["stable:com.example.a/1.0.0/x86_64/main"], kChecksumKeptA);
    EXPECT_EQ(refs["stable:com.example.b/1.0.0/x86_64/main"], kChecksumKeptB);
    EXPECT_EQ(refs["stable:com.example.c/1.0.0/x86_64/main"], kChecksumKeptC);

    // The legacy refs themselves are never rewritten by this step.
    EXPECT_EQ(refs["com.example.a/1.0.0/x86_64/main"], kChecksumLegacyA);
    EXPECT_EQ(refs["com.example.b/1.0.0/x86_64/main"], kChecksumLegacyB);
    EXPECT_EQ(refs["com.example.c/1.0.0/x86_64/main"], kChecksumLegacyC);
}

TEST(MigrateTest, MigrationAddsOnlyMissingPrefixedRefs)
{
    TempDir dir;
    std::ofstream{ dir.path() / ".version" } << "1.5.0";

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(OstreeRepo) repo = createBareRepo(dir.path() / "repo", &gErr);
    ASSERT_NE(repo, nullptr) << (gErr ? gErr->message : "ostree_repo_create failed");

    // Three legacy refs, but only B already has its prefixed counterpart. The
    // migration must create prefixed A and C from the legacy commits and must
    // keep the commit that stable:B already points at.
    ASSERT_TRUE(writeRefs(repo,
                          {
                            { nullptr, kLegacyRefA, kChecksumLegacyA },
                            { nullptr, kLegacyRefB, kChecksumLegacyB },
                            { nullptr, kLegacyRefC, kChecksumLegacyC },
                            { "stable", kLegacyRefB, kChecksumKeptB },
                          },
                          &gErr))
      << (gErr ? gErr->message : "writing refs failed");

    createLegacyLayer(dir.path(), kLegacyRefA);
    createLegacyLayer(dir.path(), kLegacyRefC);

    EXPECT_EQ(tryMigrate(dir.path(), makeConfig()), MigrateResult::Success);

    auto refs = readRefs(repo, &gErr);
    ASSERT_FALSE(refs.empty()) << (gErr ? gErr->message : "listing refs failed");

    EXPECT_EQ(refs["stable:com.example.a/1.0.0/x86_64/main"], kChecksumLegacyA);
    EXPECT_EQ(refs["stable:com.example.b/1.0.0/x86_64/main"], kChecksumKeptB);
    EXPECT_EQ(refs["stable:com.example.c/1.0.0/x86_64/main"], kChecksumLegacyC);

    // The migrated refs get a layers/<checksum> symlink pointing at the legacy
    // layer directory.
    EXPECT_TRUE(std::filesystem::is_symlink(dir.path() / "layers" / kChecksumLegacyA));
    EXPECT_TRUE(std::filesystem::is_symlink(dir.path() / "layers" / kChecksumLegacyC));
    // The pre-existing prefixed ref must not produce a symlink of its own.
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "layers" / kChecksumKeptB));
}

TEST(MigrateTest, VersionNewerThan170SkipsLegacyRefMigration)
{
    TempDir dir;
    // 2.0.0 has a larger major but smaller minor and patch than 1.7.0. The old
    // non-lexicographic comparison ordered it below 1.7.0 and re-ran the
    // migration; a real ordering must skip it.
    std::ofstream{ dir.path() / ".version" } << "2.0.0";

    g_autoptr(GError) gErr = nullptr;
    g_autoptr(OstreeRepo) repo = createBareRepo(dir.path() / "repo", &gErr);
    ASSERT_NE(repo, nullptr) << (gErr ? gErr->message : "ostree_repo_create failed");

    ASSERT_TRUE(writeRefs(repo, { { nullptr, kLegacyRefA, kChecksumLegacyA } }, &gErr))
      << (gErr ? gErr->message : "writing refs failed");

    createLegacyLayer(dir.path(), kLegacyRefA);

    EXPECT_EQ(tryMigrate(dir.path(), makeConfig()), MigrateResult::Success);

    auto refs = readRefs(repo, &gErr);
    ASSERT_FALSE(refs.empty()) << (gErr ? gErr->message : "listing refs failed");

    // The prefixed ref was not created and the legacy ref keeps its commit.
    EXPECT_TRUE(refs.find("stable:com.example.a/1.0.0/x86_64/main") == refs.end());
    EXPECT_EQ(refs["com.example.a/1.0.0/x86_64/main"], kChecksumLegacyA);
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "layers" / kChecksumLegacyA));
}

} // namespace
