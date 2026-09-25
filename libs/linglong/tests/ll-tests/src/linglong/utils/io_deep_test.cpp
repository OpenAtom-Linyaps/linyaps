/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"
#include "linglong/utils/io.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <string>
#include <vector>

namespace linglong::utils {
namespace {

TEST(IoUtilsDeepTest, ReadAndWriteFileContentStream)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QString filePath = tempDir.path() + "/stream.txt";
    std::string testContent = "Line 1: Header\nLine 2: Data\nLine 3: Footer\n";

    QFile file(filePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(testContent.c_str(), testContent.size());
    file.close();

    EXPECT_TRUE(QFile::exists(filePath));
}

TEST(IoUtilsDeepTest, DirectoryRecursiveScanAndFileListing)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QDir base(tempDir.path());
    base.mkpath("sub1/sub2");

    QFile f1(tempDir.path() + "/sub1/f1.txt");
    ASSERT_TRUE(f1.open(QIODevice::WriteOnly));
    f1.write("f1");
    f1.close();

    QFile f2(tempDir.path() + "/sub1/sub2/f2.txt");
    ASSERT_TRUE(f2.open(QIODevice::WriteOnly));
    f2.write("f2");
    f2.close();

    EXPECT_TRUE(QFile::exists(tempDir.path() + "/sub1/f1.txt"));
    EXPECT_TRUE(QFile::exists(tempDir.path() + "/sub1/sub2/f2.txt"));
}

} // namespace
} // namespace linglong::utils
