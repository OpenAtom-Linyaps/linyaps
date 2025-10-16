/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace linglong::common::strings {

enum class splitOption : uint8_t {
    None,
    TrimWhitespace,
    SkipEmpty,
};

inline splitOption operator|(splitOption a, splitOption b)
{
    return static_cast<splitOption>(std::underlying_type_t<splitOption>(a)
                                    | std::underlying_type_t<splitOption>(b));
}

inline splitOption operator&(splitOption a, splitOption b)
{
    return static_cast<splitOption>(std::underlying_type_t<splitOption>(a)
                                    & std::underlying_type_t<splitOption>(b));
}

bool stringEqual(std::string_view str1, std::string_view str2, bool caseSensitive = false) noexcept;

std::string trim(std::string_view str, std::string_view chars = " ") noexcept;

std::vector<std::string> split(const std::string &str,
                               char delimiter,
                               splitOption option = splitOption::None) noexcept;

std::string join(const std::vector<std::string> &strs, char delimiter = ' ') noexcept;

std::string replaceSubstring(std::string_view str,
                             std::string_view from,
                             std::string_view to) noexcept;

bool starts_with(std::string_view str, std::string_view prefix) noexcept;

bool ends_with(std::string_view str, std::string_view suffix) noexcept;

bool contains(std::string_view str, std::string_view suffix) noexcept;

std::string quoteBashArg(std::string arg) noexcept;

} // namespace linglong::common::strings
