/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "consoletext.h"
#include <stdio.h>
#include <iostream>

namespace linglong {
namespace util {

void printTitle(const QString &text)
{
    std::cout << LYEL << text.toStdString() << RESET << std::endl;
}

void printText(const QString &text)
{
    std::cout << LCYN << "  " << text.toStdString() << RESET << std::endl;
}

void printReplacedText(const QString &text)
{
    std::cout << ERASE_A_LINE << "  " << LCYN << text.toStdString() << RESET << std::flush;
}

} // namespace util
} // namespace linglong