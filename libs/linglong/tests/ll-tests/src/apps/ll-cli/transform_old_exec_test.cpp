/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include <gtest/gtest.h>

#include "transform_old_exec.h"

TEST(TransformOldExec, NormalArgs)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.deepin.demo";
    char arg3[] = "bash";
    char *argv[] = { arg0, arg1, arg2, arg3 };

    auto result = transformOldExec(4, argv);

    // CLI11 processes from back, so order is reversed: argv[3], argv[2], argv[1]
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "bash");
    EXPECT_EQ(result[1], "org.deepin.demo");
    EXPECT_EQ(result[2], "run");
}

TEST(TransformOldExec, ReplacesExecWithDoubleDash)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.deepin.demo";
    char arg3[] = "--exec";
    char arg4[] = "bash";
    char *argv[] = { arg0, arg1, arg2, arg3, arg4 };

    auto result = transformOldExec(5, argv);

    ASSERT_EQ(result.size(), 4);
    EXPECT_EQ(result[0], "bash");
    EXPECT_EQ(result[1], "--");
    EXPECT_EQ(result[2], "org.deepin.demo");
    EXPECT_EQ(result[3], "run");
}

TEST(TransformOldExec, NullArgv)
{
    auto result = transformOldExec(0, nullptr);

    ASSERT_EQ(result.size(), 0);
}

TEST(TransformOldExec, NegativeArgc)
{
    char arg0[] = "ll-cli";
    char *argv[] = { arg0 };

    auto result = transformOldExec(-1, argv);

    ASSERT_EQ(result.size(), 0);
}

TEST(TransformOldExec, NoArgsAfterProgramName)
{
    char arg0[] = "ll-cli";
    char *argv[] = { arg0 };

    auto result = transformOldExec(1, argv);

    ASSERT_EQ(result.size(), 0);
}

TEST(TransformOldExec, OnlyExec)
{
    char arg0[] = "ll-cli";
    char arg1[] = "--exec";
    char *argv[] = { arg0, arg1 };

    auto result = transformOldExec(2, argv);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "--");
}

TEST(TransformOldExec, MultipleExec)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "--exec";
    char arg3[] = "cmd1";
    char arg4[] = "--exec";
    char arg5[] = "cmd2";
    char *argv[] = { arg0, arg1, arg2, arg3, arg4, arg5 };

    auto result = transformOldExec(6, argv);

    ASSERT_EQ(result.size(), 5);
    EXPECT_EQ(result[0], "cmd2");
    EXPECT_EQ(result[1], "--");
    EXPECT_EQ(result[2], "cmd1");
    EXPECT_EQ(result[3], "--");
    EXPECT_EQ(result[4], "run");
}
