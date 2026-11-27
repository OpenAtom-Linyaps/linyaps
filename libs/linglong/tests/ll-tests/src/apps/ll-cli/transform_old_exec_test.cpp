/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
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

    // CLI11 parses the vector from the back, so order is reversed.
    ASSERT_EQ(result.size(), 3U);
    EXPECT_EQ(result[0], "bash");
    EXPECT_EQ(result[1], "org.deepin.demo");
    EXPECT_EQ(result[2], "run");
}

TEST(TransformOldExec, ReplacesLegacyExecSeparator)
{
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.deepin.demo";
    char arg3[] = "--exec";
    char arg4[] = "bash";
    char *argv[] = { arg0, arg1, arg2, arg3, arg4 };

    auto result = transformOldExec(5, argv);

    ASSERT_EQ(result.size(), 4U);
    EXPECT_EQ(result[0], "bash");
    EXPECT_EQ(result[1], "--");
    EXPECT_EQ(result[2], "org.deepin.demo");
    EXPECT_EQ(result[3], "run");
}

TEST(TransformOldExec, PreservesExecAfterExplicitSeparator)
{
    // ll-cli run org.example.App -- tool --exec value
    // The guest program must receive a literal `--exec`.
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "org.example.App";
    char arg3[] = "--";
    char arg4[] = "tool";
    char arg5[] = "--exec";
    char arg6[] = "value";
    char *argv[] = { arg0, arg1, arg2, arg3, arg4, arg5, arg6 };

    auto result = transformOldExec(7, argv);

    ASSERT_EQ(result.size(), 6U);
    EXPECT_EQ(result[0], "value");
    EXPECT_EQ(result[1], "--exec");
    EXPECT_EQ(result[2], "tool");
    EXPECT_EQ(result[3], "--");
    EXPECT_EQ(result[4], "org.example.App");
    EXPECT_EQ(result[5], "run");
}

TEST(TransformOldExec, RewritesLegacyExecBeforeExplicitSeparator)
{
    // ll-cli run app --exec bash -- tool --exec value
    // Only the legacy separator before `--` is rewritten.
    char arg0[] = "ll-cli";
    char arg1[] = "run";
    char arg2[] = "app";
    char arg3[] = "--exec";
    char arg4[] = "bash";
    char arg5[] = "--";
    char arg6[] = "tool";
    char arg7[] = "--exec";
    char arg8[] = "value";
    char *argv[] = { arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };

    auto result = transformOldExec(9, argv);

    ASSERT_EQ(result.size(), 8U);
    EXPECT_EQ(result[0], "value");
    EXPECT_EQ(result[1], "--exec");
    EXPECT_EQ(result[2], "tool");
    EXPECT_EQ(result[3], "--");
    EXPECT_EQ(result[4], "bash");
    EXPECT_EQ(result[5], "--");
    EXPECT_EQ(result[6], "app");
    EXPECT_EQ(result[7], "run");
}

TEST(TransformOldExec, NullArgv)
{
    auto result = transformOldExec(0, nullptr);
    EXPECT_TRUE(result.empty());
}

TEST(TransformOldExec, ProgramNameOnly)
{
    char arg0[] = "ll-cli";
    char *argv[] = { arg0 };

    auto result = transformOldExec(1, argv);
    EXPECT_TRUE(result.empty());
}
