/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDebug>
#include <QDir>

#include "src/module/util/uuid.h"

TEST(Moduel_Util, uuid)
{
    auto uuid = linglong::util::genUuid();
    EXPECT_EQ(uuid.size() > 0, true);
}
