/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDir>

#include "module/util/xdg.h"

using namespace linglong::util;

TEST(Moduel_Util, Tool)
{
    QString env = "${HOME}/Desktop:${HOME}/Desktop";
    QRegExp exp("(\\$\\{.*\\})");
    exp.setMinimal(true);
    exp.indexIn(env);

    EXPECT_EQ(exp.capturedTexts().size(), 2);
    EXPECT_EQ(exp.capturedTexts().value(0), "${HOME}");

    auto envs = linglong::util::parseEnvKeyValue(env, ":");

    EXPECT_EQ(envs.first, envs.second);
    EXPECT_EQ(envs.first.contains("${HOME}"), false);
}

TEST(Moduel_Util, convertSpecialCharacters)
{
    QStringList args1 = {"/home/qwe/Video/test test/test.mp4"};
    QStringList args2 = {"/home/qwe/Video/test/测试 音乐.mp4"};

    auto retArgs1 = linglong::util::convertSpecialCharacters(args1);
    auto retArgs2 = linglong::util::convertSpecialCharacters(args2);

    EXPECT_EQ(retArgs1.value(0), "/home/qwe/Video/test\\ test/test.mp4");
    EXPECT_EQ(retArgs2.value(0), "/home/qwe/Video/test/测试\\ 音乐.mp4");

    auto retExec1 = parseExec(args1.value(0));
    auto retExec2 = parseExec(retArgs1.value(0));

    EXPECT_EQ(retExec1.size(), 2);
    EXPECT_EQ(retExec2.size(), 1);
}