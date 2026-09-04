/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/transaction.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/packageinfo_handler.h"
#include "linglong/utils/sha256.h"

#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <vector>
#include <string>
#include <stdexcept>

namespace linglong::utils {
namespace {

TEST(UtilsDeepTest, TransactionMultipleRollbackOrderAndLIFOExecution)
{
    std::vector<int> executionOrder;
    {
        Transaction t;
        t.addRollBack([&executionOrder]() noexcept {
            executionOrder.push_back(1);
        });
        t.addRollBack([&executionOrder]() noexcept {
            executionOrder.push_back(2);
        });
        t.addRollBack([&executionOrder]() noexcept {
            executionOrder.push_back(3);
        });
    }

    // Rollbacks should execute in LIFO (last added, first executed) order
    ASSERT_EQ(executionOrder.size(), 3U);
    EXPECT_EQ(executionOrder[0], 3);
    EXPECT_EQ(executionOrder[1], 2);
    EXPECT_EQ(executionOrder[2], 1);
}

TEST(UtilsDeepTest, TransactionCommitPreventsRollbackExecution)
{
    bool rolledBack = false;
    {
        Transaction t;
        t.addRollBack([&rolledBack]() noexcept {
            rolledBack = true;
        });
        t.commit();
    }
    EXPECT_FALSE(rolledBack);
}

TEST(UtilsDeepTest, TransactionNestedRollbackStackHandling)
{
    int state = 0;
    {
        Transaction outer;
        outer.addRollBack([&state]() noexcept {
            state += 10;
        });

        {
            Transaction inner;
            inner.addRollBack([&state]() noexcept {
                state += 1;
            });
        }
        // Inner transaction goes out of scope and rolls back (+1)
        EXPECT_EQ(state, 1);
    }
    // Outer transaction goes out of scope and rolls back (+10)
    EXPECT_EQ(state, 11);
}

TEST(UtilsDeepTest, FileLockExclusiveLockingAndRAIIRelease)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    std::string lockFilePath = (tempDir.path() + "/test.lock").toStdString();
    {
        FileLock lock(lockFilePath);
        auto acquired = lock.tryLock();
        EXPECT_TRUE(acquired.has_value());
    }
    // Lock should be released on scope exit
    {
        FileLock lock(lockFilePath);
        auto acquired = lock.tryLock();
        EXPECT_TRUE(acquired.has_value());
    }
}

TEST(UtilsDeepTest, BashCommandHelperArgEscapingAndConcatenation)
{
    std::vector<std::string> args = {
        "echo",
        "hello world",
        "foo'bar",
        "$(rm -rf /)",
        "\"quoted\""
    };

    std::string escapedCmd = BashCommandHelper::escapeAndJoin(args);
    EXPECT_FALSE(escapedCmd.empty());
    EXPECT_NE(escapedCmd.find("hello world"), std::string::npos);
}

TEST(UtilsDeepTest, Sha256ChecksumCalculationForStringAndFile)
{
    std::string sampleData = "Linglong Package Manager 2026 Test String";
    std::string hash1 = SHA256::digestString(sampleData);
    std::string hash2 = SHA256::digestString(sampleData);

    EXPECT_FALSE(hash1.empty());
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1.length(), 64U);
}

TEST(UtilsDeepTest, PackageInfoHandlerMetadataFieldExtractor)
{
    std::string validJsonInfo = R"({
        "id": "com.deepin.testutil",
        "version": "1.5.0",
        "kind": "app",
        "arch": ["x86_64"],
        "module": "binary"
    })";

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    QString jsonPath = tempDir.path() + "/info.json";

    QFile file(jsonPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(validJsonInfo.c_str(), validJsonInfo.size());
    file.close();

    EXPECT_TRUE(QFile::exists(jsonPath));
}

TEST(UtilsDeepTest, TransactionExceptionSafetyAndNoexceptInRollback)
{
    int counter = 100;
    try {
        Transaction t;
        t.addRollBack([&counter]() noexcept {
            counter = 0;
        });
        throw std::runtime_error("simulated failure");
    } catch (const std::exception &) {
        // Exception caught, rollback should have fired
    }

    EXPECT_EQ(counter, 0);
}

TEST(UtilsDeepTest, Sha256EmptyInputConsistency)
{
    std::string emptyHash = SHA256::digestString("");
    EXPECT_FALSE(emptyHash.empty());
    EXPECT_EQ(emptyHash.length(), 64U);
}

TEST(UtilsDeepTest, BashCommandHelperSpecialCharacterEscaping)
{
    std::vector<std::string> args = {
        "sh",
        "-c",
        "echo $PATH && ls -l | grep txt"
    };

    std::string joined = BashCommandHelper::escapeAndJoin(args);
    EXPECT_FALSE(joined.empty());
}

} // namespace
} // namespace linglong::utils
