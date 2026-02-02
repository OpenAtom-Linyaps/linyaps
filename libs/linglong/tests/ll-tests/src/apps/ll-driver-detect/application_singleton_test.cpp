// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "application_singleton.h"
#include "common/tempdir.h"

#include <filesystem>
#include <memory>

class ApplicationSingletonTest : public ::testing::Test
{
};

TEST_F(ApplicationSingletonTest, CanAcquireLockWhenNoOneHasIt)
{
    TempDir temp_dir;
    auto lockFilePath = temp_dir.path() / "ll_driver_detect_singleton_test.lock";
    using namespace linglong::driver::detect;
    ApplicationSingleton singleton(lockFilePath.string());

    auto result = singleton.tryAcquireLock();
    ASSERT_TRUE(result.has_value())
      << "Failed to try to acquire lock: " << result.error().message();
    EXPECT_TRUE(*result);
    EXPECT_TRUE(singleton.isLockHeld());
    EXPECT_TRUE(std::filesystem::exists(lockFilePath));
}

TEST_F(ApplicationSingletonTest, CannotAcquireLockWhenAnotherInstanceHoldsIt)
{
    using namespace linglong::driver::detect;

    TempDir temp_dir;
    auto lockFilePath = temp_dir.path() / "ll_driver_detect_singleton_test.lock";
    // First instance acquires the lock
    ApplicationSingleton singleton1(lockFilePath.string());
    auto result1 = singleton1.tryAcquireLock();
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(*result1);
    ASSERT_TRUE(singleton1.isLockHeld());

    // Second instance tries to acquire the same lock
    ApplicationSingleton singleton2(lockFilePath.string());
    auto result2 = singleton2.tryAcquireLock();

    // It should fail to acquire the lock
    ASSERT_FALSE(result2);
}

TEST_F(ApplicationSingletonTest, CanAcquireLockAfterItIsReleased)
{
    TempDir temp_dir;
    auto lockFilePath = temp_dir.path() / "ll_driver_detect_singleton_test.lock";
    using namespace linglong::driver::detect;
    // Create a scope for the first singleton
    {
        ApplicationSingleton singleton1(lockFilePath.string());
        auto result1 = singleton1.tryAcquireLock();
        ASSERT_TRUE(result1.has_value());
        ASSERT_TRUE(*result1);
        ASSERT_TRUE(singleton1.isLockHeld());
    } // singleton1 is destroyed here, releasing the lock

    // A new instance should now be able to acquire the lock
    ApplicationSingleton singleton2(lockFilePath.string());
    auto result2 = singleton2.tryAcquireLock();
    ASSERT_TRUE(result2.has_value())
      << "tryAcquireLock failed unexpectedly: " << result2.error().message();
    EXPECT_TRUE(*result2);
    EXPECT_TRUE(singleton2.isLockHeld());
}

TEST_F(ApplicationSingletonTest, ReleasingLockManuallyWorks)
{
    TempDir temp_dir;
    auto lockFilePath = temp_dir.path() / "ll_driver_detect_singleton_test.lock";
    using namespace linglong::driver::detect;
    ApplicationSingleton singleton1(lockFilePath.string());
    auto result1 = singleton1.tryAcquireLock();
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(*result1);
    ASSERT_TRUE(singleton1.isLockHeld());

    singleton1.releaseLock();
    EXPECT_FALSE(singleton1.isLockHeld());

    // A new instance should now be able to acquire the lock
    ApplicationSingleton singleton2(lockFilePath.string());
    auto result2 = singleton2.tryAcquireLock();
    ASSERT_TRUE(result2.has_value())
      << "tryAcquireLock failed unexpectedly: " << result2.error().message();
    EXPECT_TRUE(*result2);
    EXPECT_TRUE(singleton2.isLockHeld());
}

TEST_F(ApplicationSingletonTest, CreatesDirectoryForLockFile)
{
    TempDir temp_dir;
    auto lockFilePath = temp_dir.path() / "ll_driver_detect_singleton_test.lock";
    using namespace linglong::driver::detect;
    auto deepLockPath = std::filesystem::temp_directory_path() / "deep_dir_for_test" / "test.lock";
    std::filesystem::remove(deepLockPath);
    std::filesystem::remove(deepLockPath.parent_path());

    ASSERT_FALSE(std::filesystem::exists(deepLockPath.parent_path()));

    ApplicationSingleton singleton(deepLockPath.string());
    auto result = singleton.tryAcquireLock();
    ASSERT_TRUE(result.has_value())
      << "Failed to try to acquire lock: " << result.error().message();
    EXPECT_TRUE(*result);
    EXPECT_TRUE(singleton.isLockHeld());
    EXPECT_TRUE(std::filesystem::exists(deepLockPath));

    // Cleanup
    std::filesystem::remove(deepLockPath);
    std::filesystem::remove(deepLockPath.parent_path());
}
