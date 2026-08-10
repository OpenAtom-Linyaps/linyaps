// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "configure.h"
#include "linglong/repo/migrate.h"
#include "linglong/utils/filelock.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

class MigrateTest : public ::testing::Test
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

TEST_F(MigrateTest, NewerRepoVersionIsNotDowngraded)
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

TEST_F(MigrateTest, CompatibilityCheckRejectsNewerVersion)
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

TEST_F(MigrateTest, CompatibilityCheckDoesNotMigrateOlderVersion)
{
    writeVersionFile("1.5.0");

    auto result = linglong::repo::checkRepoCompatibility(root);

    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange);
    EXPECT_EQ(readVersionFile(), "1.5.0");
}

TEST_F(MigrateTest, OlderVersionWithLargerPatchIsMigrated)
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

TEST_F(MigrateTest, SameVersionReturnsNoChange)
{
    writeVersionFile(LINGLONG_VERSION);
    auto cfg = makeConfig();

    auto result = linglong::repo::tryMigrate(root, cfg);
    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange)
      << "tryMigrate should return NoChange when versions match";

    EXPECT_EQ(readVersionFile(), LINGLONG_VERSION);
}

TEST_F(MigrateTest, NonExistentRootReturnsNoChange)
{
    auto nonExistentRoot = root / "does_not_exist";
    auto cfg = makeConfig();

    auto result = linglong::repo::tryMigrate(nonExistentRoot, cfg);
    EXPECT_EQ(result, linglong::repo::MigrateResult::NoChange)
      << "tryMigrate should return NoChange for non-existent root";
}

TEST_F(MigrateTest, VersionUpdateLeavesNoTemporaryFile)
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

TEST_F(MigrateTest, CompatibilityCheckWaitsForMigrationLock)
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
