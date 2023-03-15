/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/remote_ref.h"

TEST(Module_Package, RemoteRef01)
{
    linglong::package::refact::RemoteRef ref("deepin/channel:appId/version/arch/module");

    EXPECT_EQ(ref.repo.toStdString(), "deepin");
    EXPECT_EQ(ref.channel.toStdString(), "channel");
    EXPECT_EQ(ref.packageID.toStdString(), "appId");
    EXPECT_EQ(ref.version.toStdString(), "version");
    EXPECT_EQ(ref.arch.toStdString(), "arch");
    EXPECT_EQ(ref.module.toStdString(), "module");

    EXPECT_EQ(ref.isVaild(), true);
}

TEST(Module_Package, RemoteRef02)
{
    linglong::package::refact::RemoteRef ref("deepin/cha/nnel:appId/version/arch/module");
    EXPECT_EQ(ref.isVaild(), false);
}
