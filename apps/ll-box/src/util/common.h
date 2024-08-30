/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <cstdarg>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

namespace linglong {
namespace util {

typedef std::vector<std::string> str_vec;

str_vec str_spilt(const std::string &s, const std::string &sep);

std::string str_vec_join(const str_vec &vec, char sep);

inline std::string format(const std::string fmt, ...)
{
    int n = ((int)fmt.size()) * 2;
    std::unique_ptr<char[]> formatted;
    va_list ap;
    while (true) {
        formatted.reset(new char[n]);
        strcpy(&formatted[0], fmt.c_str());
        va_start(ap, fmt);
        int final_n = vsnprintf(&formatted[0], n, fmt.c_str(), ap);
        va_end(ap);
        if (final_n < 0 || final_n >= n)
            n += abs(final_n - n + 1);
        else
            break;
    }
    return std::string{ formatted.get() };
}

} // namespace util
} // namespace linglong

template<typename T>
std::ostream &operator<<(std::ostream &out, const std::vector<T> &v)
{
    if (!v.empty()) {
        out << '[';
        std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, ", "));
        out << "\b\b]";
    }
    return out;
}
