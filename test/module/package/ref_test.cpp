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

#include "module/package/info.h"

using namespace linglong::package;

TEST(Moduel_Package, Ref)
{
    Ref ref("deepin/channel:app/1.0/la");

    EXPECT_EQ(ref.repo, "deepin");
    EXPECT_EQ(ref.id, "app");
}
