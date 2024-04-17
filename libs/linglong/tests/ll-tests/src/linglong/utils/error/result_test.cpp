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
        LINGLONG_TRACE("test LINGLONG_ERR");
        return LINGLONG_ERR("message", -1);
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    ASSERT_EQ(res.error().message().contains("LINGLONG_ERR"), true);
    ASSERT_EQ(res.error().message().contains("message"), true);
}

TEST(Error, Wrap)
{
    auto res = []() -> Result<void> {
        auto fn = []() -> Result<void> {
            LINGLONG_TRACE("test LINGLONG_ERR");
            return LINGLONG_ERR("message1", -1);
        };

        auto res = fn();
        if (!res) {
            LINGLONG_TRACE("test LINGLONG_ERR");
            return LINGLONG_ERR("message2", res);
        }

        return LINGLONG_OK;
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    ASSERT_EQ(res.error().message().contains("LINGLONG_ERR"), true);
    ASSERT_EQ(res.error().message().contains("message1"), true);
    ASSERT_EQ(res.error().message().contains("message2"), true);

    res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        return LINGLONG_ERR(std::runtime_error("runtime error"));
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    ASSERT_EQ(res.error().message().contains("runtime error"), true);

    res = []() -> Result<void> {
        LINGLONG_TRACE("test LINGLONG_ERR");
        return LINGLONG_ERR("error");
    }();

    ASSERT_EQ(res.has_value(), false);
    ASSERT_EQ(res.error().code(), -1);
    ASSERT_EQ(res.error().message().contains("error"), true);
}
