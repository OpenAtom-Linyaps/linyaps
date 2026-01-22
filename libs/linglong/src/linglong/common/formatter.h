/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include <fmt/format.h>
#include <glib.h>

#include <QString>
#include <QStringList>

template <>
struct fmt::formatter<QString> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const QString &qstr, fmt::format_context &ctx) const
      -> fmt::format_context::iterator;
#else
    auto format(const QString &qstr, fmt::format_context &ctx) -> fmt::format_context::iterator;
#endif
};

template <>
struct fmt::formatter<QStringList> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const QStringList &qstrList, fmt::format_context &ctx) const
      -> fmt::format_context::iterator;
#else
    auto format(const QStringList &qstrList, fmt::format_context &ctx)
      -> fmt::format_context::iterator;
#endif
};

template <>
struct fmt::formatter<GError> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const GError &error, fmt::format_context &ctx) const
      -> fmt::format_context::iterator;
#else
    auto format(const GError &error, fmt::format_context &ctx) -> fmt::format_context::iterator;
#endif
};
