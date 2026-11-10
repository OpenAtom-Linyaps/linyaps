// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "common/test_process.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/log/formatter.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

using namespace linglong::utils::filelock;
using namespace linglong::utils::error;

// Test fixture for FileLock tests
class FileLockTest : public ::testing::Test
{
};

using linglong::test::TestProcess;

TEST_F(FileLockTest, TryLockFailureInterProcess)
{
    TempDir temporary;
    const auto path = temporary.path() / "test_filelock.lock";
    auto lock = FileLock::create(path, LockType::Write);
    ASSERT_TRUE(lock);
    ASSERT_TRUE(lock->lock(LockType::Write));

    TestProcess child({ "filelock", "write-busy", path.string() });
    ASSERT_EQ(child.wait(), 0) << child.stderrOutput();
    EXPECT_TRUE(lock->isLocked());
}

TEST_F(FileLockTest, TryLockForFailure)
{
    TempDir temporary;
    const auto path = temporary.path() / "test_filelock.lock";
    auto lock = FileLock::create(path, LockType::Write);
    ASSERT_TRUE(lock);
    ASSERT_TRUE(lock->lock(LockType::Write));

    TestProcess child({ "filelock", "timeout", path.string() });
    ASSERT_EQ(child.wait(), 0) << child.stderrOutput();
}

TEST_F(FileLockTest, MultipleReadLocks)
{
    TempDir temporary;
    const auto path = temporary.path() / "test_filelock.lock";
    auto lock = FileLock::create(path, LockType::Read);
    ASSERT_TRUE(lock);
    ASSERT_TRUE(lock->lock(LockType::Read));

    TestProcess child({ "filelock", "read-shared", path.string() });
    ASSERT_EQ(child.wait(), 0) << child.stderrOutput();
}

TEST_F(FileLockTest, WriteLockExclusive)
{
    TempDir temporary;
    const auto path = temporary.path() / "test_filelock.lock";
    auto lock = FileLock::create(path, LockType::Write);
    ASSERT_TRUE(lock);
    ASSERT_TRUE(lock->lock(LockType::Write));

    TestProcess child({ "filelock", "write-exclusive", path.string() });
    ASSERT_EQ(child.wait(), 0) << child.stderrOutput();
}

// Test creating FileLock with create_if_missing = true
TEST_F(FileLockTest, CreateWithMissingFile)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, true);
    ASSERT_TRUE(result);
    EXPECT_TRUE(std::filesystem::exists(temp_path));

    const auto &lock = std::move(result).value();
    EXPECT_EQ(lock.type(), LockType::Read);
    EXPECT_FALSE(lock.isLocked());
}

// Test creating FileLock without create_if_missing when file missing
TEST_F(FileLockTest, CreateWithoutMissingFileFails)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, false);
    ASSERT_FALSE(result);
}

// Test creating FileLock on existing file
TEST_F(FileLockTest, CreateOnExistingFile)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    std::ofstream file(temp_path);
    file.close();

    auto result = FileLock::create(temp_path, LockType::Read, false);
    ASSERT_TRUE(result);

    const auto &lock = std::move(result).value();
    EXPECT_FALSE(lock.isLocked());
}

// Test lock and unlock for Read lock
TEST_F(FileLockTest, LockUnlockRead)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, true);
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
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Write, true);
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
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Write, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto try_result = lock.tryLock(LockType::Write);
    ASSERT_TRUE(try_result);
    EXPECT_TRUE(*try_result);
    EXPECT_TRUE(lock.isLocked());

    EXPECT_TRUE(lock.unlock());
}

// Test tryLockFor with timeout success
TEST_F(FileLockTest, TryLockForSuccess)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Write, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();

    auto try_for_result = lock.tryLockFor(LockType::Write, std::chrono::milliseconds(100));
    ASSERT_TRUE(try_for_result);
    EXPECT_TRUE(*try_for_result);
    EXPECT_TRUE(lock.isLocked());
}

TEST_F(FileLockTest, ConcurrentIndependentInstancesKeepTheirRegistrations)
{
    TempDir tempDir;
    constexpr int workerCount = 8;
    constexpr int iterations = 100;
    std::promise<void> start;
    auto started = start.get_future().share();
    std::atomic_int failures{ 0 };
    std::vector<std::thread> workers;
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&, worker] {
            const auto path = tempDir.path() / (std::to_string(worker) + ".lock");
            started.wait();
            for (int iteration = 0; iteration < iterations; ++iteration) {
                auto lock = FileLock::create(path, LockType::Write);
                if (!lock) {
                    ++failures;
                    continue;
                }
                if (!lock->lock(LockType::Write)) {
                    ++failures;
                }
                if (FileLock::create(path, LockType::Write, false)) {
                    ++failures;
                }
                std::this_thread::yield();
                auto moved = std::move(*lock);
                if (!moved.isLocked()) {
                    ++failures;
                }
            }
        });
    }
    start.set_value();
    for (auto &worker : workers) {
        worker.join();
    }
    EXPECT_EQ(failures.load(), 0);
    for (int worker = 0; worker < workerCount; ++worker) {
        EXPECT_TRUE(FileLock::create(tempDir.path() / (std::to_string(worker) + ".lock"),
                                     LockType::Write,
                                     false));
    }
}

TEST_F(FileLockTest, ConcurrentSamePathCreationHasExactlyOneOwner)
{
    TempDir tempDir;
    const auto path = tempDir.path() / "shared.lock";
    constexpr int workerCount = 8;
    std::promise<void> start;
    auto started = start.get_future().share();
    std::promise<void> release;
    auto released = release.get_future().share();
    std::promise<void> attempted;
    auto allAttempted = attempted.get_future();
    std::atomic_int attempts{ 0 };
    std::atomic_int owners{ 0 };
    std::vector<std::thread> workers;
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back([&] {
            started.wait();
            auto lock = FileLock::create(path, LockType::Write);
            if (lock) {
                ++owners;
            }
            if (++attempts == workerCount) {
                attempted.set_value();
            }
            released.wait();
        });
    }
    start.set_value();
    const auto status = allAttempted.wait_for(std::chrono::seconds(5));
    release.set_value();
    for (auto &worker : workers) {
        worker.join();
    }
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_EQ(owners.load(), 1);
    EXPECT_TRUE(FileLock::create(path, LockType::Write, false));
}

TEST_F(FileLockTest, InstanceCanBeTransferredToAnotherThread)
{
    TempDir tempDir;
    const auto path = tempDir.path() / "transferred.lock";
    auto lock = FileLock::create(path, LockType::Write);
    ASSERT_TRUE(lock);
    ASSERT_TRUE(lock->lock(LockType::Write));

    std::thread worker([owned = std::move(*lock)] {
        EXPECT_TRUE(owned.isLocked());
    });
    worker.join();

    auto reopened = FileLock::create(path, LockType::Write, false);
    ASSERT_TRUE(reopened);
    EXPECT_TRUE(reopened->tryLock(LockType::Write).value_or(false));
}

// Test cannot create duplicate lock in same process
TEST_F(FileLockTest, NoDuplicateLockSameProcess)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result1 = FileLock::create(temp_path, LockType::Read, true);
    ASSERT_TRUE(result1);

    auto result2 = FileLock::create(temp_path, LockType::Read, false);
    ASSERT_FALSE(result2);
    EXPECT_THAT(result2.error().message(),
                ::testing::HasSubstr("process already holds a lock on file"));
}

// Test move constructor
TEST_F(FileLockTest, MoveConstructor)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Write, true);
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
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result1 = FileLock::create(temp_path, LockType::Write, true);
    ASSERT_TRUE(result1);
    auto lock1 = std::move(result1).value();
    EXPECT_TRUE(lock1.lock(LockType::Write));

    const auto other_path = temp_dir.path() / "other.lock";
    auto result2 = FileLock::create(other_path, LockType::Write, true);
    ASSERT_TRUE(result2);
    auto lock2 = std::move(result2).value();

    lock2 = std::move(lock1);
    EXPECT_TRUE(lock2.isLocked());
    EXPECT_EQ(lock2.type(), LockType::Write);
    EXPECT_FALSE(lock1.isLocked());

    auto reopened = FileLock::create(other_path, LockType::Write, false);
    ASSERT_TRUE(reopened) << fmt::format("{}", reopened.error());
}

// Test tryLock when already locked same type
TEST_F(FileLockTest, TryLockAlreadyLockedSameType)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Read));

    auto try_result = lock.tryLock(LockType::Read);
    ASSERT_TRUE(try_result);
    EXPECT_TRUE(*try_result);
    EXPECT_TRUE(lock.isLocked());
}

// Test tryLockFor when already holding a compatible lock
TEST_F(FileLockTest, TryLockForAlreadyLockedSameType)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Read));

    auto try_for_result = lock.tryLockFor(LockType::Read, std::chrono::milliseconds(100));
    ASSERT_TRUE(try_for_result);
    EXPECT_TRUE(*try_for_result);
    EXPECT_TRUE(lock.isLocked());
}

// Test lock when already locked different type
TEST_F(FileLockTest, LockAlreadyLockedDiffType)
{
    TempDir temp_dir;
    auto temp_path = temp_dir.path() / "test_filelock.lock";
    auto result = FileLock::create(temp_path, LockType::Read, true);
    ASSERT_TRUE(result);
    auto lock = std::move(result).value();
    EXPECT_TRUE(lock.lock(LockType::Read));

    auto lock_result = lock.lock(LockType::Write);
    ASSERT_FALSE(lock_result);
    EXPECT_THAT(lock_result.error().message(),
                ::testing::HasSubstr("failed to lock with different type"));
}

namespace linglong::test {

int runFileLockHelper(const std::vector<std::string> &arguments)
{
    if (arguments.size() != 2
        || (arguments[0] != "write-busy" && arguments[0] != "timeout"
            && arguments[0] != "read-shared" && arguments[0] != "write-exclusive")) {
        std::cerr << "invalid filelock helper arguments\n";
        return 2;
    }

    const auto type = arguments[0] == "read-shared" ? LockType::Read : LockType::ReadWrite;
    auto lock = FileLock::create(arguments[1], type, false);
    if (!lock) {
        std::cerr << lock.error().message() << '\n';
        return 1;
    }

    const auto check = [](const auto &result, bool expected) {
        if (!result) {
            std::cerr << result.error().message() << '\n';
            return false;
        }
        if (*result != expected) {
            std::cerr << "unexpected lock availability\n";
            return false;
        }
        return true;
    };

    if (arguments[0] == "timeout") {
        return check(lock->tryLockFor(LockType::Write, std::chrono::milliseconds(100)), false) ? 0
                                                                                               : 1;
    }
    if (arguments[0] == "read-shared") {
        return check(lock->tryLock(LockType::Read), true) ? 0 : 1;
    }
    if (arguments[0] == "write-exclusive" && !check(lock->tryLock(LockType::Read), false)) {
        return 1;
    }

    return check(lock->tryLock(LockType::Write), false) ? 0 : 1;
}

} // namespace linglong::test
