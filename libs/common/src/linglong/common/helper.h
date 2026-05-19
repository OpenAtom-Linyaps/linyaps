// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

namespace linglong::common::helper {
template <typename... T>
struct Overload : T...
{
    using T::operator()...;
};

template <typename... T>
Overload(T...) -> Overload<T...>;
}; // namespace linglong::common::helper
