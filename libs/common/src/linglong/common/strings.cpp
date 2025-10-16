/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/common/strings.h"

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

std::string trim(std::string_view str, std::string_view chars) noexcept
{
    auto first = str.find_first_not_of(chars);
    if (first == std::string_view::npos) {
        return "";
    }

    auto last = str.find_last_not_of(chars);
    return std::string(str.substr(first, last - first + 1));
}

std::vector<std::string> split(const std::string &str, char delimiter, splitOption option) noexcept
{
    std::vector<std::string> result;
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
            result.push_back(std::move(token));
        }

        start = end + 1;
    }

    auto token = str.substr(start);
    if (trimWhitespace) {
        token = trim(token);
    }

    if (!skipEmpty || !token.empty()) {
        result.push_back(std::move(token));
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

} // namespace linglong::common::strings
