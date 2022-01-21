/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDir>
#include <QDebug>

TEST(Moduel_Util, FS)
{
    QDir empty("");

    EXPECT_EQ(empty.exists(), true);
}
