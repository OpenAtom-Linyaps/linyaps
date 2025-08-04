/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "formatter.h"

#if FMT_VERSION >= 70000
auto fmt::formatter<QString>::format(const QString &qstr, fmt::format_context &ctx) const
  -> fmt::format_context::iterator
#else
auto fmt::formatter<QString>::format(const QString &qstr, fmt::format_context &ctx)
  -> fmt::format_context::iterator
#endif
{
    return formatter<std::string_view>::format(qstr.toStdString(), ctx);
}

#if FMT_VERSION >= 70000
auto fmt::formatter<QStringList>::format(const QStringList &qstrList,
                                         fmt::format_context &ctx) const
  -> fmt::format_context::iterator
#else
auto fmt::formatter<QStringList>::format(const QStringList &qstrList, fmt::format_context &ctx)
  -> fmt::format_context::iterator
#endif
{
    return formatter<std::string_view>::format(qstrList.join(" ").toStdString(), ctx);
}

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
      fmt::format("[code {}] backtrace:\n{}", error.code(), error.message()),
      ctx);
}