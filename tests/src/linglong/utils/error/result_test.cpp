/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/utils/error/error.h"

using namespace linglong::utils::error;

TEST(Error, New)
{
    auto res = []() -> Result<void> {
        return LINGLONG_ERR(-1, "message");
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
}

TEST(Error, Wrap)
{
    auto res = []() -> Result<void> {
        auto fn = []() -> Result<void> {
            return LINGLONG_ERR(-1, "message1");
        };

        auto res = fn();
        if (!res.has_value()) {
            return LINGLONG_EWRAP("message2", res.error());
        }
        return LINGLONG_OK;
    }();

    ASSERT_EQ(res.has_value(), false);
}
