// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include <sys/wait.h>
#include <unistd.h>

using namespace linglong::utils::filelock;
using namespace linglong::utils::error;

namespace {
// Helper function to create a temporary file path
std::filesystem::path GetTempFilePath()
{
    return std::filesystem::temp_directory_path() / ("test_filelock.lock");
}
} // namespace

// Test fixture for FileLock tests
class FileLockTest : public ::testing::Test
{
protected:
    std::filesystem::path temp_path;

    void SetUp() override
    {
        temp_path = GetTempFilePath();
        // Ensure the file does not exist initially
        std::filesystem::remove(temp_path);
    }

    void TearDown() override { std::filesystem::remove(temp_path); }
};

// Test creating FileLock with create_if_missing = true
TEST_F(FileLockTest, CreateWithMissingFile)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(temp_path));

    const auto &lock = std::move(result).value();
    EXPECT_EQ(lock.type(), LockType::Read); // Default type
    EXPECT_FALSE(lock.isLocked());
}

// Test creating FileLock without create_if_missing when file missing
TEST_F(FileLockTest, CreateWithoutMissingFileFails)
{
    auto result = FileLock::create(temp_path, false);
    ASSERT_FALSE(result);
}

// Test creating FileLock on existing file
TEST_F(FileLockTest, CreateOnExistingFile)
{
    std::ofstream file(temp_path);
    file.close();

    auto result = FileLock::create(temp_path, false);
    ASSERT_TRUE(result);

    const auto &lock = std::move(result).value();
    EXPECT_FALSE(lock.isLocked());
}

// Test lock and unlock for Read lock
TEST_F(FileLockTest, LockUnlockRead)
{
    auto result = FileLock::create(temp_path);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto lock_result = lock.lock(LockType::Read);
    ASSERT_TRUE(lock_result) << lock_result.error().message();
    EXPECT_TRUE(lock.isLocked());
    EXPECT_EQ(lock.type(), LockType::Read);

    auto unlock_result = lock.unlock();
    ASSERT_TRUE(unlock_result);
    EXPECT_FALSE(lock.isLocked());
}

// Test lock and unlock for Write lock
TEST_F(FileLockTest, LockUnlockWrite)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto lock_result = lock.lock(LockType::Write);
    ASSERT_TRUE(lock_result);
    EXPECT_TRUE(lock.isLocked());
    EXPECT_EQ(lock.type(), LockType::Write);

    auto unlock_result = lock.unlock();
    ASSERT_TRUE(unlock_result);
    EXPECT_FALSE(lock.isLocked());
}

// Test tryLock success
TEST_F(FileLockTest, TryLockSuccess)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto try_result = lock.tryLock(LockType::Write);
    ASSERT_TRUE(try_result);
    EXPECT_TRUE(*try_result);
    EXPECT_TRUE(lock.isLocked());

    EXPECT_TRUE(lock.unlock());
}

// Test tryLock failure when already locked by another process
TEST_F(FileLockTest, TryLockFailureInterProcess)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock1 = std::move(result).value();

    EXPECT_TRUE(lock1.lock(LockType::Write));
    EXPECT_TRUE(lock1.isLocked());

    const pid_t pid = fork();
    if (pid == 0) { // Child process
        auto result_child = FileLock::create(temp_path, false);
        if (!result_child) {
            _exit(1);
        }
        auto lock_child = std::move(result_child).value();

        auto try_result = lock_child.tryLock(LockType::Write);
        if (!try_result || *try_result) {
            _exit(1);
        }

        _exit(0);
    } else if (pid > 0) { // Parent
        int status{ 0 };
        EXPECT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_EQ(WEXITSTATUS(status), 0);
    } else {
        FAIL() << "Fork failed";
    }
}

// Test tryLockFor with timeout success
TEST_F(FileLockTest, TryLockForSuccess)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto try_for_result = lock.tryLockFor(LockType::Write, std::chrono::milliseconds(100));
    ASSERT_TRUE(try_for_result);
    EXPECT_TRUE(*try_for_result);
    EXPECT_TRUE(lock.isLocked());
}

// Test tryLockFor with timeout failure
TEST_F(FileLockTest, TryLockForFailure)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock1 = std::move(result).value();

    EXPECT_TRUE(lock1.lock(LockType::Write));
    EXPECT_TRUE(lock1.isLocked());

    const pid_t pid = fork();
    if (pid == 0) { // Child
        auto result_child = FileLock::create(temp_path, false);
        if (!result_child) {
            _exit(1);
        }
        auto lock_child = std::move(result_child).value();

        auto try_for_result =
          lock_child.tryLockFor(LockType::Write, std::chrono::milliseconds(100));
        if (!try_for_result || *try_for_result) {
            _exit(1);
        }
        _exit(0);
    } else if (pid > 0) {
        int status{ 0 };
        EXPECT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_EQ(WEXITSTATUS(status), 0);
    } else {
        FAIL() << "Fork failed";
    }
}

// Test relock from Read to Write
TEST_F(FileLockTest, RelockReadToWrite)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    EXPECT_TRUE(lock.lock(LockType::Read));
    EXPECT_EQ(lock.type(), LockType::Read);

    auto relock_result = lock.relock(LockType::Write);
    ASSERT_TRUE(relock_result);
    EXPECT_EQ(lock.type(), LockType::Write);
    EXPECT_TRUE(lock.isLocked());
}

// Test relock from Write to Read
TEST_F(FileLockTest, RelockWriteToRead)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    EXPECT_TRUE(lock.lock(LockType::Write));
    EXPECT_EQ(lock.type(), LockType::Write);

    auto relock_result = lock.relock(LockType::Read);
    ASSERT_TRUE(relock_result);
    EXPECT_EQ(lock.type(), LockType::Read);
    EXPECT_TRUE(lock.isLocked());
}

// Test relock when not locked fails
TEST_F(FileLockTest, RelockNotLocked)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto relock_result = lock.relock(LockType::Write);
    // Since not locked, relock should just return OK if same type, but test behavior
    // Actually, relock assumes locked, but code checks if type == new_type, returns OK
    // But to test properly:
    EXPECT_TRUE(
      relock_result); // Since type_ is Read by default, changing to Write would attempt to lock
    // Wait, relock is for changing type while locked.
    // If not locked, it shouldn't be called, but let's see code: relock doesn't check if locked.
    // It just sets the new flock.
    // So if not locked, it would effectively lock it.
    auto lock_result = lock.lock(LockType::Read);
    ASSERT_TRUE(lock_result);
    auto relock = lock.relock(LockType::Read);
    ASSERT_TRUE(relock);
}

// Test multiple read locks possible (shared)
TEST_F(FileLockTest, MultipleReadLocks)
{
    auto result1 = FileLock::create(temp_path);
    ASSERT_TRUE(result1);
    auto lock1 = std::move(result1).value();
    EXPECT_TRUE(lock1.lock(LockType::Read));

    const auto pid = ::fork();
    if (pid == 0) {
        auto result2 = FileLock::create(temp_path);
        if (!result2) {
            _exit(1);
        }

        auto lock2 = std::move(result2).value();
        if (auto ret = lock2.lock(LockType::Read); !ret) {
            _exit(2);
        }

        _exit(0);
    } else if (pid > 0) {
        int status{ 0 };
        EXPECT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_EQ(WEXITSTATUS(status), 0);
    } else {
        FAIL() << "Fork failed";
    }
}

// Test write lock exclusive
TEST_F(FileLockTest, WriteLockExclusive)
{
    auto result1 = FileLock::create(temp_path);
    ASSERT_TRUE(result1);
    auto lock1 = std::move(result1).value();
    EXPECT_TRUE(lock1.lock(LockType::Write));

    const auto pid = ::fork();
    if (pid == 0) {
        auto result2 = FileLock::create(temp_path, false);
        if (!result2) {
            _exit(1);
        }
        auto lock2 = std::move(result2).value();

        auto try_read = lock2.tryLock(LockType::Read);
        if (!try_read || *try_read) {
            _exit(2);
        }

        auto try_write = lock2.tryLock(LockType::Write);
        if (!try_write || *try_write) {
            _exit(3);
        }

        _exit(0);
    } else if (pid > 0) {
        int status{ 0 };
        EXPECT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_EQ(WEXITSTATUS(status), 0);
    } else {
        FAIL() << "Fork failed";
    }
}

// Test behavior after fork
TEST_F(FileLockTest, LockAfterFork)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Write));

    const pid_t pid = fork();
    if (pid == 0) { // Child
        // In child, lock should not be considered locked because pid changed
        if (lock.isLocked()) {
            _exit(1);
        }

        auto lock_result = lock.lock(LockType::Write);
        if (lock_result) { // Should fail because "cannot operate on lock from forked process"
            _exit(2);
        }

        _exit(0);
    } else if (pid > 0) {
        int status{ 0 };
        EXPECT_EQ(waitpid(pid, &status, 0), pid);
        EXPECT_EQ(WEXITSTATUS(status), 0);
        EXPECT_TRUE(lock.isLocked()); // Parent still locked
    } else {
        FAIL() << "Fork failed";
    }
}

// Test cannot create duplicate lock in same process
TEST_F(FileLockTest, NoDuplicateLockSameProcess)
{
    auto result1 = FileLock::create(temp_path, true);
    ASSERT_TRUE(result1);

    auto result2 = FileLock::create(temp_path, false);
    ASSERT_FALSE(result2);
    EXPECT_THAT(result2.error().message(),
                ::testing::HasSubstr("process already holds a lock on file"));
}

// Test move constructor
TEST_F(FileLockTest, MoveConstructor)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock1 = std::move(result).value();
    EXPECT_TRUE(lock1.lock(LockType::Write));

    auto lock2(std::move(lock1));
    EXPECT_TRUE(lock2.isLocked());
    EXPECT_FALSE(lock1.isLocked()); // Moved from
}

// Test move assignment
TEST_F(FileLockTest, MoveAssignment)
{
    auto result1 = FileLock::create(temp_path, true);
    ASSERT_TRUE(result1);
    auto lock1 = std::move(result1).value();
    EXPECT_TRUE(lock1.lock(LockType::Write));

    const std::filesystem::path other_path =
      std::filesystem::temp_directory_path() / ("other_" + std::to_string(::getpid()) + ".lock");
    auto result2 = FileLock::create(other_path, true);
    ASSERT_TRUE(result2);
    auto lock2 = std::move(result2).value();

    lock2 = std::move(lock1);
    EXPECT_TRUE(lock2.isLocked());
    EXPECT_EQ(lock2.type(), LockType::Write);
    EXPECT_FALSE(lock1.isLocked());
}

// Test tryLock when already locked same type
TEST_F(FileLockTest, TryLockAlreadyLockedSameType)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Read));

    auto try_result = lock.tryLock(LockType::Read);
    ASSERT_TRUE(try_result);
}

// Test lock when already locked different type
TEST_F(FileLockTest, LockAlreadyLockedDiffType)
{
    auto result = FileLock::create(temp_path, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Read));

    auto lock_result = lock.lock(LockType::Write);
    ASSERT_FALSE(lock_result);
    EXPECT_THAT(lock_result.error().message(),
                ::testing::HasSubstr("use relock to change lock type"));
}
