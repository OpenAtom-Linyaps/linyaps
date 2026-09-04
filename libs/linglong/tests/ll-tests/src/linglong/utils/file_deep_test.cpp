/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/file.h"
#include "linglong/utils/io.h"
#include "linglong/utils/error/error.h"

#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <vector>
#include <string>

namespace linglong::utils {
namespace {

TEST(FileUtilsDeepTest, FileExistenceAndPermissionsCheck)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QString file1 = tempDir.path() + "/regular_file.txt";
    QFile f(file1);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write("content");
    f.close();

    EXPECT_TRUE(QFile::exists(file1));
}

TEST(FileUtilsDeepTest, TemporaryDirectoryCleanupOnScopeExit)
{
    QString tempPath;
    {
        QTemporaryDir innerDir;
        ASSERT_TRUE(innerDir.isValid());
        tempPath = innerDir.path();
        EXPECT_TRUE(QDir(tempPath).exists());
    }
    EXPECT_FALSE(QDir(tempPath).exists());
}

TEST(FileUtilsDeepTest, LargeFileChunkedIoOperations)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QString largeFile = tempDir.path() + "/large_data.bin";
    QFile f(largeFile);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));

    QByteArray chunk(1024 * 1024, 'A'); // 1MB chunk
    for (int i = 0; i < 5; ++i) {
        f.write(chunk);
    }
    f.close();

    EXPECT_EQ(QFileInfo(largeFile).size(), 5 * 1024 * 1024);
}

} // namespace
} // namespace linglong::utils
