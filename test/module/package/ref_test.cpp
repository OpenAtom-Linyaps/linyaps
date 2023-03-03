/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/ref.h"

TEST(Moduel_Package, Ref)
{
    linglong::package::Ref ref("deepin:channel/appId/version/arch/module");

    EXPECT_EQ(ref.repo.toStdString(), "deepin");
    EXPECT_EQ(ref.channel.toStdString(), "channel");
    EXPECT_EQ(ref.appId.toStdString(), "appId");
    EXPECT_EQ(ref.version.toStdString(), "version");
    EXPECT_EQ(ref.arch.toStdString(), "arch");
    EXPECT_EQ(ref.module.toStdString(), "module");
}
