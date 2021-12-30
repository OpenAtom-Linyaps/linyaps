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
