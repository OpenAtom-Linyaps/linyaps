/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDir>
#include <QDebug>

#include "src/module/util/runner.h"

TEST(Moduel_Util, runner)
{
    auto ret = linglong::runner::Runner("ostree1", {"--repo=/tmp/linglongtest", "init", "--mode=bare-user-only"}, 3000);
    EXPECT_EQ (ret, false);

    ret = linglong::runner::Runner("ostree", {"--repo=/tmp/linglongtest", "init", "--mode=bare-user-only1"}, 3000);
    EXPECT_EQ (ret, false);

}
