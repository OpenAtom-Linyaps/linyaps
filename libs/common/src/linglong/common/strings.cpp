/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/common/strings.h"

#include <fmt/format.h>

#include <sstream>

namespace {
int hexToBin(char c) noexcept
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return -1;
}
} // namespace

namespace linglong::common::strings {

bool stringEqual(std::string_view str1, std::string_view str2, bool caseSensitive) noexcept
{
    if (caseSensitive) {
        return str1 == str2;
    }

    return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), [](char ch1, char ch2) {
        return tolower(ch1) == tolower(ch2);
    });
}

std::string_view trim(std::string_view str, std::string_view chars) noexcept
{
    auto first = str.find_first_not_of(chars);
    if (first == std::string_view::npos) {
        return {};
    }

    auto last = str.find_last_not_of(chars);
    return str.substr(first, last - first + 1);
}

std::vector<std::string_view> split(std::string_view str,
                                    char delimiter,
                                    splitOption option) noexcept
{
    std::vector<std::string_view> result;
    std::size_t start{ 0 };
    std::size_t end{ 0 };
    auto trimWhitespace = (option & splitOption::TrimWhitespace) != splitOption::None;
    auto skipEmpty = (option & splitOption::SkipEmpty) != splitOption::None;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        auto token = str.substr(start, end - start);
        if (trimWhitespace) {
            token = trim(token);
        }

        if (!skipEmpty || !token.empty()) {
            result.push_back(token);
        }

        start = end + 1;
    }

    auto token = str.substr(start);
    if (trimWhitespace) {
        token = trim(token);
    }

    if (!skipEmpty || !token.empty()) {
        result.push_back(token);
    }

    return result;
}

std::string join(const std::vector<std::string> &strs, char delimiter) noexcept
{
    if (strs.empty()) {
        return "";
    }

    if (strs.size() == 1) {
        return strs[0];
    }

    size_t total_len = strs.size() - 1;
    for (const auto &s : strs) {
        total_len += s.size();
    }

    std::string result;
    result.reserve(total_len);
    result.append(strs[0]);
    for (size_t i = 1; i < strs.size(); ++i) {
        result.push_back(delimiter);
        result.append(strs[i]);
    }

    return result;
}

std::string replaceSubstring(std::string_view str,
                             std::string_view from,
                             std::string_view to) noexcept
{
    std::string result;
    if (from.empty()) {
        return std::string(str);
    }

    size_t start = 0;
    while (true) {
        const size_t pos = str.find(from, start);
        if (pos == std::string_view::npos) {
            break;
        }
        // Append the part before the match
        result.append(str.data() + start, pos - start);
        // Append the replacement
        result.append(to.data(), to.size());
        // Move past the matched part
        start = pos + from.size();
    }
    // Append the remaining part of the string
    result.append(str.data() + start, str.size() - start);
    return result;
}

bool starts_with(std::string_view str, std::string_view prefix) noexcept
{
    if (str.size() < prefix.size()) {
        return false;
    }

    return str.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view str, std::string_view suffix) noexcept
{
    if (str.size() < suffix.size()) {
        return false;
    }

    return str.substr(str.size() - suffix.size()) == suffix;
}

bool contains(std::string_view str, std::string_view suffix) noexcept
{
    return str.find(suffix) != std::string_view::npos;
}

// Quotes a string for serializing arguments to a bash script.
// Example:
//   Input:  "let's go"
//   Output: "'let'\''s go'"
std::string quoteBashArg(std::string arg) noexcept
{
    const std::string quotePrefix = "'\\";
    for (auto it = arg.begin(); it != arg.end(); it++) {
        if (*it == '\'') {
            it = arg.insert(it, quotePrefix.cbegin(), quotePrefix.cend());
            it = arg.insert(it + quotePrefix.size() + 1, 1, '\'');
        }
    }
    return "'" + arg + "'";
}

std::optional<std::string> decode_url(std::string_view url) noexcept
{
    std::string result;
    result.reserve(url.size());

    for (std::size_t idx = 0; idx < url.size(); ++idx) {
        if (auto ch = url.at(idx); ch != '%') {
            result.push_back(ch);
            continue;
        }

        if (idx + 2 >= url.size()) {
            return std::nullopt;
        }

        auto h = hexToBin(url.at(idx + 1));
        auto l = hexToBin(url.at(idx + 2));
        if (h < 0 || l < 0) {
            return std::nullopt;
        }

        auto high{ static_cast<std::byte>(h) };
        auto low{ static_cast<std::byte>(l) };

        auto ch = (high << 4) | low;
        result.push_back(static_cast<char>(ch));

        idx += 2;
    }

    return result;
}

std::string encode_url(std::string_view value) noexcept
{
    std::string escaped;
    escaped.reserve(value.length());

    for (unsigned char c : value) {
        if (::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '/') {
            escaped.push_back(static_cast<char>(c));
        } else {
            fmt::format_to(std::back_inserter(escaped), "%{:02X}", c);
        }
    }

    return escaped;
}

} // namespace linglong::common::strings
