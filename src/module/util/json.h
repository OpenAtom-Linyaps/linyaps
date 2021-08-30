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

#include <nlohmann/json.hpp>

#include <fstream>

namespace util {
namespace json {

inline nlohmann::json fromByteArray(const std::string &content)
{
    return nlohmann::json::parse(content);
}

inline nlohmann::json fromFile(const std::string &filepath)
{
    std::ifstream f(filepath);
    std::string str((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    auto j = fromByteArray(str);
    return j;
}

} // namespace json
} // namespace util