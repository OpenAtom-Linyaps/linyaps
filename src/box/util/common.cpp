/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

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
