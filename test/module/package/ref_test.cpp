/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/ref.h"

TEST(Moduel_Package, Ref)
{
    linglong::package::Ref ref("deepin/channel:app/1.0/la");

    EXPECT_EQ(ref.repo, "deepin");
    EXPECT_EQ(ref.appId, "app");
}
