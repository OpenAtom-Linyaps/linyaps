/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <QDebug>

inline auto operator<<(QDebug &out, const std::string &str) -> QDebug &
{
    out << QString::fromStdString(str);
    return out;
}
