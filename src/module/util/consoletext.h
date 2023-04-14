/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_CONSOLETEXT_H_
#define LINGLONG_SRC_MODULE_UTIL_CONSOLETEXT_H_

#include <QString>

namespace linglong {
namespace util {

/* FOREGROUND */
#define RST "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define LRED "\033[0;31m"
#define LGRN "\033[1;32m"
#define LYEL "\033[1;33m"
#define LCYN "\033[0;36m"
#define LMAG "\033[0;35m"
#define RESET "\033[0m"

#define ERASE_A_LINE "\33[2K\r"

void printTitle(const QString &text);
void printText(const QString &text);
void printReplacedText(const QString &text);

} // namespace util
} // namespace linglong
#endif