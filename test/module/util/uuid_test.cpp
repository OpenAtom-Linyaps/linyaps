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

#include "src/module/util/uuid.h"

TEST(Moduel_Util, uuid)
{
    auto uuid = linglong::util::genUuid();
    EXPECT_EQ(uuid.size() > 0, true);
}
