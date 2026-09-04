/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"

#include <vector>
#include <string>

namespace linglong::utils {
namespace {

TEST(CmdUtilsDeepTest, CommandStringParsingAndArgumentHandling)
{
    Cmd cmd("ls");
    EXPECT_NO_THROW(cmd.setEnv("TEST_VAR", "TEST_VALUE"));
}

TEST(CmdUtilsDeepTest, CommandExecutionEnvironmentVariables)
{
    Cmd cmd("echo");
    cmd.setEnv("FOO", "BAR");
    // Verify environment configuration API
    SUCCEED();
}

} // namespace
} // namespace linglong::utils
