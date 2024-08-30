/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "nlohmann/json.hpp"

#include <optional>

namespace nlohmann {

template<class J, class T>
inline void from_json(const J &j, std::optional<T> &v)
{
    if (j.is_null()) {
        v = std::nullopt;
    } else {
        v = j.template get<T>();
    }
}

template<class J, class T>
inline void to_json(J &j, const std::optional<T> &o)
{
    if (o.has_value()) {
        j = o.value();
    }
}

} // namespace nlohmann

namespace linglong {

template<class T>
std::optional<T> optional(const nlohmann::json &j, const char *key)
{
    std::optional<T> o;
    auto iter = j.template find(key);
    if (iter != j.end()) {
        // check object is empty: {}, skip it
        if (iter->is_object() && iter->size() == 0) {
            return o;
        }

        o = iter->template get<std::optional<T>>();
    }
    return o;
}

} // namespace linglong
