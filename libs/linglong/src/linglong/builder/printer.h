/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#ifndef LINGLONG_SRC_BUILDER_PRINTER_H_
#define LINGLONG_SRC_BUILDER_PRINTER_H_
namespace linglong::builder {

void printMessage(const std::string &text, const size_t num = 0)
{
    std::string blank(num, ' ');

    std::cout << blank << text << std::endl;
}

void printReplacedText(const std::string &text, const size_t num = 0)
{
    std::string blank(num, ' ');

    std::cout << "\33[2K\r" << blank << text << std::flush;
}
}

#endif // LINGLONG_SRC_BUILDER_PRINTER_H_
