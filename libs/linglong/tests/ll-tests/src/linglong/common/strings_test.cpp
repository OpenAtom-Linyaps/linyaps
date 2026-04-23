// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
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
    EXPECT_EQ(split("a,b,c", ',', splitOption::None),
              std::vector<std::string_view>({ "a", "b", "c" }));
    EXPECT_EQ(split(" a , b , c ", ',', splitOption::TrimWhitespace),
              std::vector<std::string_view>({ "a", "b", "c" }));
    EXPECT_EQ(split("a,,c", ',', splitOption::SkipEmpty),
              std::vector<std::string_view>({ "a", "c" }));
    EXPECT_EQ(split(" a , , c ", ',', splitOption::TrimWhitespace | splitOption::SkipEmpty),
              std::vector<std::string_view>({ "a", "c" }));
    EXPECT_EQ(split(",a,b,", ',', splitOption::SkipEmpty),
              std::vector<std::string_view>({ "a", "b" }));
    EXPECT_EQ(split("a,b,", ',', splitOption::None),
              std::vector<std::string_view>({ "a", "b", "" }));
    EXPECT_EQ(split("", ',', splitOption::None), std::vector<std::string_view>({ "" }));
    EXPECT_EQ(split("", ',', splitOption::SkipEmpty), std::vector<std::string_view>({}));
    EXPECT_EQ(split(",,", ',', splitOption::None), std::vector<std::string_view>({ "", "", "" }));
    EXPECT_EQ(split(",,", ',', splitOption::SkipEmpty), std::vector<std::string_view>({}));
    EXPECT_EQ(split("a=", '=', splitOption::None), std::vector<std::string_view>({ "a", "" }));
    EXPECT_EQ(split("a", '=', splitOption::None), std::vector<std::string_view>({ "a" }));
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

TEST(StringsTest, EncodeUrl)
{
    EXPECT_EQ(encode_url(""), "");
    EXPECT_EQ(encode_url("hello"), "hello");
    EXPECT_EQ(encode_url("hello-world"), "hello-world");
    EXPECT_EQ(encode_url("hello_world"), "hello_world");
    EXPECT_EQ(encode_url("hello.world"), "hello.world");
    EXPECT_EQ(encode_url("hello/world"), "hello/world");
    EXPECT_EQ(encode_url("hello123"), "hello123");
    EXPECT_EQ(encode_url("hello world"), "hello%20world");
    EXPECT_EQ(encode_url("hello!world"), "hello%21world");
    EXPECT_EQ(encode_url("hello@world"), "hello%40world");
    EXPECT_EQ(encode_url("hello#world"), "hello%23world");
    EXPECT_EQ(encode_url("hello%world"), "hello%25world");
    EXPECT_EQ(encode_url("hello&world"), "hello%26world");
    EXPECT_EQ(encode_url("hello=world"), "hello%3Dworld");
    EXPECT_EQ(encode_url("hello+world"), "hello%2Bworld");
    EXPECT_EQ(encode_url("hello?world"), "hello%3Fworld");
    EXPECT_EQ(encode_url(" "), "%20");
    EXPECT_EQ(encode_url("\n"), "%0A");
    EXPECT_EQ(encode_url("\t"), "%09");
    EXPECT_EQ(encode_url("\x01"), "%01");
    EXPECT_EQ(encode_url("\xFF"), "%FF");
}

TEST(StringsTest, DecodeUrl)
{
    EXPECT_EQ(decode_url(""), "");
    EXPECT_EQ(decode_url("hello"), "hello");
    EXPECT_EQ(decode_url("hello%20world"), "hello world");
    EXPECT_EQ(decode_url("%20"), " ");
    EXPECT_EQ(decode_url("%00"), std::string(1, '\0'));
    EXPECT_EQ(decode_url("%FF"), std::string(1, '\xFF'));
    EXPECT_EQ(decode_url("%ff"), std::string(1, '\xFF'));
    EXPECT_EQ(decode_url("%aF"), std::string(1, '\xAF'));
    EXPECT_EQ(decode_url("%Af"), std::string(1, '\xAF'));
    EXPECT_EQ(decode_url("hello%21world"), "hello!world");
    EXPECT_EQ(decode_url("%21%22%23"), "!\"#");
    EXPECT_EQ(decode_url("a%20b%20c"), "a b c");
    EXPECT_EQ(decode_url("%%"), std::nullopt);
    EXPECT_EQ(decode_url("%"), std::nullopt);
    EXPECT_EQ(decode_url("%0"), std::nullopt);
    EXPECT_EQ(decode_url("%GG"), std::nullopt);
    EXPECT_EQ(decode_url("%G0"), std::nullopt);
    EXPECT_EQ(decode_url("%0G"), std::nullopt);
    EXPECT_EQ(decode_url("hello%"), std::nullopt);
    EXPECT_EQ(decode_url("hello%2"), std::nullopt);
    EXPECT_EQ(decode_url("hello%2Gworld"), std::nullopt);
}

TEST(StringsTest, EncodeDecodeUrlRoundTrip)
{
    std::vector<std::string> testCases = {
        "hello world",           "test@example.com",
        "path/to/file",          "key=value&name=test",
        "hello world!@#$%^&*()", "你好",
        "hello\nworld",          "",
    };
    for (const auto &testCase : testCases) {
        auto encoded = encode_url(testCase);
        auto decoded = decode_url(encoded);
        EXPECT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded.value(), testCase);
    }
}

} // namespace linglong::common::strings
