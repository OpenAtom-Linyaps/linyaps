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

template <typename T>
struct ptr_view
{
    ptr_view(T *v)
        : value(v)
    {
    }

    T *value;
};

template <typename T>
struct fmt::formatter<ptr_view<T>> : fmt::formatter<std::string_view>
{
#if FMT_VERSION >= 70000
    auto format(const ptr_view<T> &ptr, fmt::format_context &ctx) const
      -> fmt::format_context::iterator
#else
    auto format(const ptr_view<T> &ptr, fmt::format_context &ctx) -> fmt::format_context::iterator
#endif
    {
        if (ptr.value == nullptr) {
            return formatter<std::string_view>::format("no error(nullptr)", ctx);
        } else {
            return formatter<std::string_view>::format(fmt::format("{}", *ptr.value), ctx);
        }
    }
};
