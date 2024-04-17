/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "common.h"

namespace linglong {
namespace util {

std::string str_vec_join(const str_vec &vec, char sep)
{
    if (vec.empty()) {
        return "";
    }

    std::string s;
    for (auto iterator = vec.begin(); iterator != std::prev(vec.end()); ++iterator) {
        s += *iterator + sep;
    }
    s += vec.back();
    return s;
}

str_vec str_spilt(const std::string &s, const std::string &sep)
{
    str_vec vec;
    size_t pos_begin = 0;
    size_t pos_end = 0;
    while ((pos_end = s.find(sep, pos_begin)) != std::string::npos) {
        auto t = s.substr(pos_begin, pos_end - pos_begin);
        if (!t.empty()) {
            vec.push_back(t);
        }
        pos_begin = pos_end + sep.size();
    }
    auto t = s.substr(pos_begin, s.size() - pos_begin);
    if (!t.empty()) {
        vec.push_back(t);
    }
    return vec;
}

} // namespace util
} // namespace linglong
