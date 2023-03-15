/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/ref_new.h"

TEST(Module_Package, Ref01)
{
    linglong::package::refact::Ref ref("appId/version/arch/module");

    EXPECT_EQ(ref.packageID.toStdString(), "appId");
    EXPECT_EQ(ref.version.toStdString(), "version");
    EXPECT_EQ(ref.arch.toStdString(), "arch");
    EXPECT_EQ(ref.module.toStdString(), "module");

    EXPECT_EQ(ref.isVaild(), true);
}

TEST(Module_Package, Ref02)
{
    linglong::package::refact::Ref ref("appId/vers@ion/arch/module");
    EXPECT_EQ(ref.isVaild(), false);
}

TEST(Module_Package, Ref03)
{
    linglong::package::refact::Ref ref("appId/vers/ion/arch/module");
    EXPECT_EQ(ref.isVaild(), false);
}