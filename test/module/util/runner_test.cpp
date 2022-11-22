/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDebug>
#include <QDir>

#include "src/module/util/runner.h"

TEST(Moduel_Util, runner)
{
    auto ret = linglong::runner::Runner("ostree1", {"--repo=/tmp/linglongtest", "init", "--mode=bare-user-only"}, 3000);
    EXPECT_EQ(ret, false);

    ret = linglong::runner::Runner("ostree", {"--repo=/tmp/linglongtest", "init", "--mode=bare-user-only1"}, 3000);
    EXPECT_EQ(ret, false);
}
