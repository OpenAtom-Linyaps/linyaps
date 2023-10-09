/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/util/runner.h"

#include <QDebug>
#include <QDir>

TEST(Module_Util, runner)
{
    auto err = linglong::util::Exec("ostree1",
                                    { "--repo=/tmp/linglongtest", "init", "--mode=bare-user-only" },
                                    3000);
    EXPECT_EQ(static_cast<bool>(err), true);

    err = linglong::util::Exec("ostree",
                               { "--repo=/tmp/linglongtest", "init", "--mode=bare-user-only1" },
                               3000);
    EXPECT_EQ(static_cast<bool>(err), true);
}
