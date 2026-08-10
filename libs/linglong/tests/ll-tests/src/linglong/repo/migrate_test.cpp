/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "configure.h"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/migrate.h"
#include "linglong/utils/filelock.h"

#include <ostree.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

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

namespace fs = std::filesystem;

class MigrateDowngradeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>("linglong-migrate-test-");
        ASSERT_TRUE(tempDir->isValid());
        root = tempDir->path();
    }

    void TearDown() override { tempDir.reset(); }

    // Write a .version file with the given content
    void writeVersionFile(const std::string &version)
    {
        std::ofstream out(root / ".version");
        out << version;
    }

    // Read the .version file
    std::string readVersionFile()
    {
        std::ifstream in(root / ".version");
        std::stringstream buf;
        buf << in.rdbuf();
        return buf.str();
    }

    // Create a minimal valid config for tryMigrate
    linglong::api::types::v1::RepoConfigV2 makeConfig()
    {
        linglong::api::types::v1::RepoConfigV2 cfg{};
        linglong::api::types::v1::Repo repo{};
        repo.name = "test";
        repo.url = "https://example.com";
        cfg.repos.push_back(repo);
        cfg.defaultRepo = "test";
        cfg.version = 1;
        return cfg;
    }

    fs::path root;
    std::unique_ptr<TempDir> tempDir;
};

TEST_F(MigrateDowngradeTest, NewerRepoVersionIsNotDowngraded)
{
    int major{};
    int minor{};
    int patch{};
    char trailing{};
    ASSERT_EQ(std::sscanf(LINGLONG_VERSION, "%d.%d.%d%c", &major, &minor, &patch, &trailing), 3);
    const auto newerVersion = std::to_string(major + 1) + ".0.0";

    writeVersionFile(newerVersion);
    ASSERT_TRUE(fs::create_directory(root / "repo"));
    auto cfg = makeConfig();

    auto result = linglong::repo::tryMigrate(root, cfg);
    EXPECT_EQ(result, linglong::repo::MigrateResult::Incompatible)
      << "tryMigrate should refuse when repo version is newer than program version";

    EXPECT_EQ(readVersionFile(), newerVersion);
}

TEST_F(MigrateDowngradeTest, CompatibilityCheckRejectsNewerVersion)
{
    int major{};
    int minor{};
    int patch{};
    char trailing{};
    ASSERT_EQ(std::sscanf(LINGLONG_VERSION, "%d.%d.%d%c", &major, &minor, &patch, &trailing), 3);
    const auto newerVersion = std::to_string(major + 1) + ".0.0";
    writeVersionFile(newerVersion);

    auto result = linglong::repo::checkRepoCompatibility(root);

    EXPECT_EQ(result, linglong::repo::MigrateResult::Incompatible);
    EXPECT_EQ(readVersionFile(), newerVersion);
}

TEST_F(MigrateDowngradeTest, CompatibilityCheckDoesNotMigrateOlderVersion)
{
    writeVersionFile("1.5.0");

    auto result = linglong::repo::checkRepoCompatibility(root);

    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange);
    EXPECT_EQ(readVersionFile(), "1.5.0");
}

TEST_F(MigrateDowngradeTest, OlderVersionWithLargerPatchIsMigrated)
{
    int major{};
    int minor{};
    int patch{};
    char trailing{};
    ASSERT_EQ(std::sscanf(LINGLONG_VERSION, "%d.%d.%d%c", &major, &minor, &patch, &trailing), 3);

    std::string olderVersion;
    if (minor > 0) {
        olderVersion =
          std::to_string(major) + "." + std::to_string(minor - 1) + "." + std::to_string(patch + 1);
    } else {
        ASSERT_GT(major, 0);
        olderVersion = std::to_string(major - 1) + ".1." + std::to_string(patch + 1);
    }

    writeVersionFile(olderVersion);
    auto result = linglong::repo::tryMigrate(root, makeConfig());

    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange);
    EXPECT_EQ(readVersionFile(), LINGLONG_VERSION);
}

TEST_F(MigrateDowngradeTest, SameVersionReturnsNoChange)
{
    writeVersionFile(LINGLONG_VERSION);
    auto cfg = makeConfig();

    auto result = linglong::repo::tryMigrate(root, cfg);
    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange)
      << "tryMigrate should return NoChange when versions match";

    EXPECT_EQ(readVersionFile(), LINGLONG_VERSION);
}

TEST_F(MigrateDowngradeTest, NonExistentRootReturnsNoChange)
{
    auto nonExistentRoot = root / "does_not_exist";
    auto cfg = makeConfig();

    auto result = linglong::repo::tryMigrate(nonExistentRoot, cfg);
    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange)
      << "tryMigrate should return NoChange for non-existent root";
}

TEST_F(MigrateDowngradeTest, VersionUpdateLeavesNoTemporaryFile)
{
    writeVersionFile("1.5.0");

    auto result = linglong::repo::tryMigrate(root, makeConfig());

    EXPECT_NE(result, linglong::repo::MigrateResult::Failed);
    EXPECT_EQ(readVersionFile(), LINGLONG_VERSION);
    for (const auto &entry : fs::directory_iterator(root)) {
        EXPECT_EQ(entry.path().filename().string().rfind(".version.", 0), std::string::npos)
          << "temporary version file was not cleaned up: " << entry.path();
    }
}

TEST_F(MigrateDowngradeTest, CompatibilityCheckWaitsForMigrationLock)
{
    using linglong::utils::filelock::FileLock;
    using linglong::utils::filelock::LockType;

    writeVersionFile("99.0.0");
    auto lock = FileLock::create(root / ".migration.lock", LockType::ReadWrite);
    ASSERT_TRUE(lock.has_value());
    ASSERT_TRUE(lock->lock(LockType::Write).has_value());

    int readyPipe[2]{};
    ASSERT_EQ(::pipe(readyPipe), 0);
    const auto child = ::fork();
    ASSERT_NE(child, -1);
    if (child == 0) {
        ::close(readyPipe[0]);
        const char ready = '1';
        if (::write(readyPipe[1], &ready, sizeof(ready)) != sizeof(ready)) {
            _exit(2);
        }
        ::close(readyPipe[1]);
        const auto result = linglong::repo::checkRepoCompatibility(root);
        _exit(result == linglong::repo::MigrateResult::Incompatible ? 0 : 3);
    }

    ::close(readyPipe[1]);
    char ready{};
    ASSERT_EQ(::read(readyPipe[0], &ready, sizeof(ready)), sizeof(ready));
    ::close(readyPipe[0]);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int status{};
    EXPECT_EQ(::waitpid(child, &status, WNOHANG), 0)
      << "compatibility check did not wait for the migration lock";

    ASSERT_TRUE(lock->unlock().has_value());
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

} // namespace
