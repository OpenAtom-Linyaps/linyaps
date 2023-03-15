/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/ref_new.h"

#include <QSharedPointer>

// ${packageID}/${version}[/${arch}[/${module}]]

TEST(Module_Package, New_Ref)
{
    using linglong::package::refact::Ref;

    QSharedPointer<Ref> ref(new Ref("appId/version"));

    EXPECT_EQ(ref->packageID.toStdString(), "appId");
    EXPECT_EQ(ref->version.toStdString(), "version");
    EXPECT_EQ(ref->arch.toStdString(), "x86_64");
    EXPECT_EQ(ref->module.toStdString(), "runtime");

    EXPECT_EQ(ref->isVaild(), true);
    ref->module = "devel";
    EXPECT_EQ(ref->isVaild(), true);
    ref->module = "de/el";
    EXPECT_EQ(ref->isVaild(), false);

    ref.reset(new Ref("appId/vers@ion/arch/module"));
    EXPECT_EQ(ref->isVaild(), false);
    ref->version = "1.1.2";
    EXPECT_EQ(ref->isVaild(), true);

    ref.reset(new Ref("-appId/version/arch/module"));
    EXPECT_EQ(ref->isVaild(), false);

    ref.reset(new Ref("appId/vers/ion/arch/module/d/d/d/d"));
    EXPECT_EQ(ref->isVaild(), false);
}
