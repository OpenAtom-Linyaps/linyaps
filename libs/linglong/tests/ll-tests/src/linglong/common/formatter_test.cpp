/*
 * SPDX-FileCopyrightText: 2026 Yanghanrui666
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/formatter.h"

#include <fmt/format.h>

#include <QString>
#include <QStringList>

#include <string>

TEST(FormatterTest, QStringFormatter)
{
    EXPECT_EQ(fmt::format("{}", QString("hello")), "hello");
    EXPECT_EQ(fmt::format("{}", QString("")), "");
    EXPECT_EQ(fmt::format("{}", QString("你好世界")), "你好世界");
    EXPECT_EQ(fmt::format("prefix: {} :suffix", QString("mid")), "prefix: mid :suffix");
    EXPECT_EQ(fmt::format("num={}", QString("42")), "num=42");
}

TEST(FormatterTest, QStringListFormatter)
{
    EXPECT_EQ(fmt::format("{}", QStringList()), "");
    EXPECT_EQ(fmt::format("{}", QStringList({ "a" })), "a");
    EXPECT_EQ(fmt::format("{}", QStringList({ "a", "b", "c" })), "a b c");
    EXPECT_EQ(fmt::format("{}", QStringList({ "", "" })), " ");
    EXPECT_EQ(fmt::format("[{}]", QStringList({ "x", "y" })), "[x y]");
}

TEST(FormatterTest, PtrViewFormatter)
{
    int normalValue = 42;
    int *normalPtr = &normalValue;
    int *nullPtr = nullptr;

    EXPECT_EQ(fmt::format("{}", ptr_view<int>(normalPtr)), "42");
    EXPECT_EQ(fmt::format("{}", ptr_view<int>(nullPtr)), "no error(nullptr)");
    EXPECT_EQ(fmt::format("err={}", ptr_view<int>(nullPtr)), "err=no error(nullptr)");

    std::string strValue = "test";
    std::string *strPtr = &strValue;
    EXPECT_EQ(fmt::format("{}", ptr_view<std::string>(strPtr)), "test");
}

TEST(FormatterTest, FormatWithMultipleArgs)
{
    EXPECT_EQ(fmt::format("{} {}", QString("hello"), QString("world")), "hello world");
    EXPECT_EQ(fmt::format("{} {}", QStringList({ "a", "b" }), ptr_view<int>(nullptr)),
              "a b no error(nullptr)");
}
