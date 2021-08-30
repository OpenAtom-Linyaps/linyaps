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

#pragma once

#include "util.h"

#include <iostream>
#include <algorithm>
#include <cstring>

struct with_separator {
    explicit with_separator(std::string sep)
        : sep(std::move(sep))
    {
    }

    std::string sep;
};

struct separated_stream {
    separated_stream(std::ostream &stream, std::string sep)
        : _stream(stream)
        , _sep(std::move(sep))
        , _first(true)
    {
    }

    template<class Rhs>
    separated_stream &operator<<(Rhs &&rhs)
    {
        if (_first)
            _first = false;
        else
            _stream << _sep;

        _stream << std::forward<Rhs>(rhs);
        return *this;
    }

    separated_stream &operator<<(std::ostream &(*manip)(std::ostream &))
    {
        manip(_stream);
        return *this;
    }

private:
    std::ostream &_stream;
    std::string _sep;
    bool _first;
};

inline separated_stream operator<<(std::ostream &stream, with_separator wsep)
{
    return separated_stream(stream, std::move(wsep.sep));
}

#define ogStdout (std::cout << with_separator(" ") << std::endl)
#define logWar() (std::cout << with_separator(" ") << std::endl)
#define logInf() (std::cout << with_separator(" ") << std::endl)
#define logDbg() (std::cout << with_separator(" ") << std::endl)
#define logErr() (std::cout << with_separator(" ") << std::endl)

//#define ogStdout (std::cout << with_separator(" ") << std::endl \
//                            << __FUNCTION__ << __LINE__)
//#define logWar() (std::cout << with_separator(" ") << std::endl \
//                            << __FUNCTION__ << __LINE__)
//#define logInf() (std::cout << with_separator(" ") << std::endl \
//                            << __FUNCTION__ << __LINE__)
//#define logDbg() (std::cout << with_separator(" ") << std::endl \
//                            << __FUNCTION__ << __LINE__)
//#define logErr() (std::cout << with_separator(" ") << std::endl \
//                            << __FUNCTION__ << __LINE__)

namespace util {

inline std::string errnoString()
{
    return util::format("errno(%d): %s", errno, strerror(errno));
}

inline std::string retErrString(int ret)
{
    return util::format("ret(%d),errno(%d): %s", ret, errno, strerror(errno));
}

} // namespace util