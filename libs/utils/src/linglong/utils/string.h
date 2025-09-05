/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace linglong::utils {

inline bool stringEqual(std::string_view str1, std::string_view str2, bool caseSensitive = false)
{
    if (caseSensitive) {
        return str1 == str2;
    }

    return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), [](char ch1, char ch2) {
        return tolower(ch1) == tolower(ch2);
    });
}

inline std::vector<std::string> splitString(std::string_view str, char delimiter)
{
    std::vector<std::string> result;
    std::string token;

    for (char ch : str) {
        if (ch == delimiter) {
            if (!token.empty()) {
                result.push_back(token);
                token.clear();
            }
        } else {
            token += ch;
        }
    }
    if (!token.empty()) {
        result.push_back(token);
    }
    return result;
}

inline std::string replaceSubstring(std::string_view str,
                                    std::string_view from,
                                    std::string_view to)
{
    std::string result;
    if (from.empty()) {
        return std::string(str);
    }

    size_t start = 0;
    while (true) {
        size_t pos = str.find(from, start);
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

} // namespace linglong::utils
