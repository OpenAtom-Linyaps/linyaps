/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "linglong/utils/error/error.h"

#include <fmt/format.h>

#include <filesystem>
#include <string_view>

template <>
struct fmt::formatter<linglong::utils::error::Error> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const linglong::utils::error::Error &error, fmt::format_context &ctx) const
      -> fmt::format_context::iterator;
#else
    auto format(const linglong::utils::error::Error &error, fmt::format_context &ctx)
      -> fmt::format_context::iterator;
#endif
};

template <>
struct fmt::formatter<std::filesystem::path> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const std::filesystem::path &path, fmt::format_context &ctx) const
      -> fmt::format_context::iterator;
#else
    auto format(const std::filesystem::path &path, fmt::format_context &ctx)
      -> fmt::format_context::iterator;
#endif
};
