/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include <iostream>
#include <string>

namespace linglong::builder {

inline void printMessage(const std::string &text, const size_t num = 0)
{
    std::string blank(num, ' ');

    std::cout << blank << text << std::endl;
}

inline void printReplacedText(const std::string &text, const size_t num = 0)
{
    std::string blank(num, ' ');

    std::cout << "\33[2K\r" << blank << text << std::flush;
}

inline void printProgress(const size_t num = 0)
{
    std::cout << "\33[2K\r"
              << "[";
    for (int i = 0; i < num; ++i) {
        std::cout << "=";
    }
    if (num != 0)
        std::cout << ">";
    else
        std::cout << " ";
    for (int i = 0; i < 100 - num; ++i) {
        std::cout << " ";
    }
    std::cout << "] " << num << "%" << std::flush;
}

} // namespace linglong::builder
