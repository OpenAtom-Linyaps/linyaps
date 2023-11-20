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
        return Err(-1, "message");
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
}

TEST(Error, Wrap)
{
    auto res = []() -> Result<void> {
        auto fn = []() -> Result<void> {
            return Err(-1, "message1");
        };

        auto res = fn();
        if (!res.has_value()) {
            return EWrap("message2", res.error());
        }
        return Ok;
    }();

    ASSERT_EQ(res.has_value(), false);
}
