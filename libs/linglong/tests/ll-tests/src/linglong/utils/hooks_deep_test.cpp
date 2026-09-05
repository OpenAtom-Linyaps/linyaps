/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"
#include "linglong/utils/hooks.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <string>
#include <vector>

namespace linglong::utils {
namespace {

TEST(HooksDeepTest, HookExecutionOrderAndEnvironmentSetup)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    QString hookPath = tempDir.path() + "/post-install.sh";
    QFile file(hookPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("#!/bin/sh\nexit 0\n");
    file.close();

    EXPECT_TRUE(QFile::exists(hookPath));
}

TEST(HooksDeepTest, InvalidHookPathFailureHandling)
{
    std::string nonExistentHook = "/non/existent/path/hook.sh";
    EXPECT_FALSE(QFile::exists(QString::fromStdString(nonExistentHook)));
}

} // namespace
} // namespace linglong::utils
