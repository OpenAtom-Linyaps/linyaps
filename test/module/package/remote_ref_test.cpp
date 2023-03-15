/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/remote_ref.h"

#include <QSharedPointer>

TEST(Module_Package, RemoteRef)
{
    using linglong::package::refact::RemoteRef;
    QSharedPointer<RemoteRef> ref(new RemoteRef("deepin/channel:appId/version/arch/module"));

    EXPECT_EQ(ref->repo.toStdString(), "deepin");
    EXPECT_EQ(ref->channel.toStdString(), "channel");
    EXPECT_EQ(ref->packageID.toStdString(), "appId");
    EXPECT_EQ(ref->version.toStdString(), "version");
    EXPECT_EQ(ref->arch.toStdString(), "arch");
    EXPECT_EQ(ref->module.toStdString(), "module");

    EXPECT_EQ(ref->isVaild(), true);

    ref.reset(new RemoteRef("deepin/cha/nnel:appId/version/arch/module"));
    EXPECT_EQ(ref->isVaild(), false);

    ref.reset(new RemoteRef(":appId/version/arch/module"));
    EXPECT_EQ(ref->isVaild(), true);
}
