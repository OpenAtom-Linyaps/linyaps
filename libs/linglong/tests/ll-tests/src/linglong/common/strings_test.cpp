// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/strings.h"

namespace linglong::common::strings {

TEST(StringsTest, StringEqual)
{
    EXPECT_TRUE(stringEqual("hello", "hello", true));
    EXPECT_FALSE(stringEqual("hello", "world", true));
    EXPECT_FALSE(stringEqual("hello", "Hello", true));
    EXPECT_TRUE(stringEqual("hello", "Hello", false));
    EXPECT_TRUE(stringEqual("HELLO", "hello", false));
    EXPECT_FALSE(stringEqual("hello", "world", false));
    EXPECT_TRUE(stringEqual("", "", true));
    EXPECT_TRUE(stringEqual("", "", false));
    EXPECT_FALSE(stringEqual("hello", "", true));
    EXPECT_FALSE(stringEqual("", "hello", false));
    EXPECT_FALSE(stringEqual("hello", "hello world", true));
}

TEST(StringsTest, Trim)
{
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("hello  "), "hello");
    EXPECT_EQ(trim("  hello"), "hello");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim("  "), "");
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("...hello...", "."), "hello");
    EXPECT_EQ(trim("abchelloabc", "abc"), "hello");
    EXPECT_EQ(trim("xxxyhelloxyx", "xy"), "hello");
    EXPECT_EQ(trim("  hello  ", "."), "  hello  ");
}

TEST(StringsTest, Split)
{
    EXPECT_EQ(split("a,b,c", ',', splitOption::None), std::vector<std::string>({ "a", "b", "c" }));
    EXPECT_EQ(split(" a , b , c ", ',', splitOption::TrimWhitespace),
              std::vector<std::string>({ "a", "b", "c" }));
    EXPECT_EQ(split("a,,c", ',', splitOption::SkipEmpty), std::vector<std::string>({ "a", "c" }));
    EXPECT_EQ(split(" a , , c ", ',', splitOption::TrimWhitespace | splitOption::SkipEmpty),
              std::vector<std::string>({ "a", "c" }));
    EXPECT_EQ(split(",a,b,", ',', splitOption::SkipEmpty), std::vector<std::string>({ "a", "b" }));
    EXPECT_EQ(split("a,b,", ',', splitOption::None), std::vector<std::string>({ "a", "b", "" }));
    EXPECT_EQ(split("", ',', splitOption::None), std::vector<std::string>({ "" }));
    EXPECT_EQ(split("", ',', splitOption::SkipEmpty), std::vector<std::string>({}));
    EXPECT_EQ(split(",,", ',', splitOption::None), std::vector<std::string>({ "", "", "" }));
    EXPECT_EQ(split(",,", ',', splitOption::SkipEmpty), std::vector<std::string>({}));
}

TEST(StringsTest, Join)
{
    EXPECT_EQ(join({ "a", "b", "c" }, ','), "a,b,c");
    EXPECT_EQ(join({ "a" }, ','), "a");
    EXPECT_EQ(join({}, ','), "");
    EXPECT_EQ(join({ "", "", "" }, ','), ",,");
}

TEST(StringsTest, ReplaceSubstring)
{
    EXPECT_EQ(replaceSubstring("hello world", "world", "linglong"), "hello linglong");
    EXPECT_EQ(replaceSubstring("abab", "ab", "c"), "cc");
    EXPECT_EQ(replaceSubstring("hello", "world", "linglong"), "hello");
    EXPECT_EQ(replaceSubstring("hello world", "world", ""), "hello ");
    EXPECT_EQ(replaceSubstring("hello", "", "linglong"), "hello");
    EXPECT_EQ(replaceSubstring("", "a", "b"), "");
}

TEST(StringsTest, StartsWith)
{
    EXPECT_TRUE(starts_with("hello world", "hello"));
    EXPECT_FALSE(starts_with("hello world", "world"));
    EXPECT_FALSE(starts_with("hello", "hello world"));
    EXPECT_TRUE(starts_with("hello", ""));
    EXPECT_FALSE(starts_with("", "hello"));
    EXPECT_TRUE(starts_with("", ""));
}

TEST(StringsTest, EndsWith)
{
    EXPECT_TRUE(ends_with("hello world", "world"));
    EXPECT_FALSE(ends_with("hello world", "hello"));
    EXPECT_FALSE(ends_with("world", "hello world"));
    EXPECT_TRUE(ends_with("hello", ""));
    EXPECT_FALSE(ends_with("", "hello"));
    EXPECT_TRUE(ends_with("", ""));
}

TEST(StringsTest, Contains)
{
    EXPECT_TRUE(contains("hello world", "lo wo"));
    EXPECT_FALSE(contains("hello world", "abc"));
    EXPECT_TRUE(contains("hello", "hello"));
    EXPECT_TRUE(contains("hello", ""));
    EXPECT_FALSE(contains("", "hello"));
    EXPECT_TRUE(contains("", ""));
}

TEST(StringsTest, QuoteBashArg)
{
    EXPECT_EQ(quoteBashArg(""), "''");
    EXPECT_EQ(quoteBashArg("hello"), "'hello'");
    EXPECT_EQ(quoteBashArg("hello world"), "'hello world'");
    EXPECT_EQ(quoteBashArg("let's go"), "'let'\\''s go'");
    EXPECT_EQ(quoteBashArg("a'b'c"), "'a'\\''b'\\''c'");
    EXPECT_EQ(quoteBashArg("'test"), "''\\''test'");
    EXPECT_EQ(quoteBashArg("test'"), "'test'\\'''");
    EXPECT_EQ(quoteBashArg("'"), "''\\'''");
    EXPECT_EQ(quoteBashArg("hello $world"), "'hello $world'");
}

} // namespace linglong::common::strings
