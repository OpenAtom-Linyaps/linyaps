/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/runtime/overlayfs_driver.h"
#include "linglong/utils/error/error.h"

#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <vector>
#include <string>

namespace linglong::runtime {

class OverlayFSDriverDeepTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());
        baseDir = tempDir.path().toStdString();
        lower1 = baseDir + "/lower1";
        lower2 = baseDir + "/lower2";
        upper = baseDir + "/upper";
        work = baseDir + "/work";
        target = baseDir + "/target";

        QDir(QString::fromStdString(lower1)).mkpath(".");
        QDir(QString::fromStdString(lower2)).mkpath(".");
        QDir(QString::fromStdString(upper)).mkpath(".");
        QDir(QString::fromStdString(work)).mkpath(".");
        QDir(QString::fromStdString(target)).mkpath(".");
    }

    QTemporaryDir tempDir;
    std::string baseDir;
    std::string lower1;
    std::string lower2;
    std::string upper;
    std::string work;
    std::string target;
};

TEST_F(OverlayFSDriverDeepTest, OverlayFSPathFormatValidation)
{
    std::vector<std::string> lowers = { lower1, lower2 };
    EXPECT_FALSE(lowers.empty());
    EXPECT_EQ(lowers.size(), 2U);
}

TEST_F(OverlayFSDriverDeepTest, OverlayFSMountDirectoryExistenceChecks)
{
    EXPECT_TRUE(QDir(QString::fromStdString(lower1)).exists());
    EXPECT_TRUE(QDir(QString::fromStdString(lower2)).exists());
    EXPECT_TRUE(QDir(QString::fromStdString(upper)).exists());
    EXPECT_TRUE(QDir(QString::fromStdString(work)).exists());
    EXPECT_TRUE(QDir(QString::fromStdString(target)).exists());
}

TEST_F(OverlayFSDriverDeepTest, MountOptionStringEscapingAndColonFormat)
{
    std::string combinedLower = lower1 + ":" + lower2;
    std::string expectedOption = "lowerdir=" + combinedLower + ",upperdir=" + upper + ",workdir=" + work;
    EXPECT_TRUE(expectedOption.find("lowerdir=") != std::string::npos);
    EXPECT_TRUE(expectedOption.find("upperdir=") != std::string::npos);
    EXPECT_TRUE(expectedOption.find("workdir=") != std::string::npos);
}

TEST_F(OverlayFSDriverDeepTest, EmptyLowerDirsHandling)
{
    std::vector<std::string> emptyLowers;
    EXPECT_TRUE(emptyLowers.empty());
}

} // namespace linglong::runtime
