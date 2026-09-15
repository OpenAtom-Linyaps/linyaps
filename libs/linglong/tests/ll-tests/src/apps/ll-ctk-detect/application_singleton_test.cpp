// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "application_singleton.h"
#include "common/tempdir.h"

#include <filesystem>

using namespace linglong::ctk::detect;

class ApplicationSingletonTest : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(ApplicationSingletonTest, CanAcquireLockWhenNoOneHoldsIt)
{
    const auto lockPath = tempDir.path() / "ll_ctk_detect_singleton_test.lock";
    ApplicationSingleton singleton(lockPath.string());

    auto result = singleton.tryAcquireLock();
    ASSERT_TRUE(result) << "failed to acquire lock: " << result.error().message();
    EXPECT_TRUE(*result);
    EXPECT_TRUE(singleton.isLockHeld());
    EXPECT_TRUE(std::filesystem::exists(lockPath));
}

TEST_F(ApplicationSingletonTest, CannotAcquireLockWhenAnotherInstanceHoldsIt)
{
    const auto lockPath = tempDir.path() / "ll_ctk_detect_singleton_test.lock";
    ApplicationSingleton first(lockPath.string());
    ASSERT_TRUE(first.tryAcquireLock());

    ApplicationSingleton second(lockPath.string());
    auto result = second.tryAcquireLock();
    ASSERT_FALSE(result);
}

TEST_F(ApplicationSingletonTest, CanAcquireLockAfterRelease)
{
    const auto lockPath = tempDir.path() / "ll_ctk_detect_singleton_test.lock";
    {
        ApplicationSingleton first(lockPath.string());
        ASSERT_TRUE(first.tryAcquireLock());
    }

    ApplicationSingleton second(lockPath.string());
    auto result = second.tryAcquireLock();
    ASSERT_TRUE(result);
    EXPECT_TRUE(*result);
}
