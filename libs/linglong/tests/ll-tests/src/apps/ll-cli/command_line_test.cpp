/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "command_line.h"

using linglong::cli::transformOldExecArguments;
using ::testing::ElementsAre;

TEST(CommandLine, ConvertsLegacyExecSeparator)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.example.App";
    char arg3[] = "--exec";
    char arg4[] = "bash";
    char *argv[]{ arg0, arg1, arg2, arg3, arg4 };

    EXPECT_THAT(transformOldExecArguments(5, argv),
                ElementsAre("bash", "--", "org.example.App", "run"));
}

TEST(CommandLine, PreservesExecArgumentAfterCommandSeparator)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.example.App";
    char arg3[] = "--";
    char arg4[] = "tool";
    char arg5[] = "--exec";
    char arg6[] = "value";
    char *argv[]{ arg0, arg1, arg2, arg3, arg4, arg5, arg6 };

    EXPECT_THAT(transformOldExecArguments(7, argv),
                ElementsAre("value", "--exec", "tool", "--", "org.example.App", "run"));
}
