/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/util/error.h"

TEST(Moduel_Util, Error)
{
    linglong::util::Error err;
    EXPECT_EQ(err.success(), true);

    auto funcLevel_1 = []() -> linglong::util::Error {
        return NewError(-1, "this is level 1 error");
    };

    err = funcLevel_1();
    EXPECT_EQ(err.success(), false);

    auto funcLevel_2 = [=]() -> linglong::util::Error {
        auto r = funcLevel_1();
        return WrapError(r, "this is level 2 error");
    };

    err = funcLevel_2();

    EXPECT_EQ(err.success(), false);
    EXPECT_EQ(err.code(), -1);

    qCritical() << err;
}
