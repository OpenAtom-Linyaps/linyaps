/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_STD_HELPER_QDEBUG_HELPER_H_
#define LINGLONG_UTILS_STD_HELPER_QDEBUG_HELPER_H_

#include <QDebug>

inline auto operator<<(QDebug &out, const std::string &str) -> QDebug &
{
    out << QString::fromStdString(str);
    return out;
}

#endif
