/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "formatter.h"

#if FMT_VERSION >= 70000
auto fmt::formatter<linglong::utils::error::Error>::format(
  const linglong::utils::error::Error &error, fmt::format_context &ctx) const
  -> fmt::format_context::iterator
#else
auto fmt::formatter<linglong::utils::error::Error>::format(
  const linglong::utils::error::Error &error, fmt::format_context &ctx)
  -> fmt::format_context::iterator
#endif
{
    return formatter<std::string_view>::format(
      fmt::format("[code {}]:\n{}", error.code(), error.message()),
      ctx);
}

#if FMT_VERSION >= 70000
auto fmt::formatter<std::filesystem::path>::format(const std::filesystem::path &path,
                                                   fmt::format_context &ctx) const
  -> fmt::format_context::iterator
#else
auto fmt::formatter<std::filesystem::path>::format(const std::filesystem::path &path,
                                                   fmt::format_context &ctx)
  -> fmt::format_context::iterator
#endif
{
    return formatter<std::string_view>::format(path.string(), ctx);
}
