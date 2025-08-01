/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "env.h"

#include <QProcessEnvironment>

namespace linglong::utils::command {

QStringList getUserEnv(const QStringList &filters)
{
    auto sysEnv = QProcessEnvironment::systemEnvironment();
    auto ret = QProcessEnvironment();
    for (const auto &filter : filters) {
        auto v = sysEnv.value(filter, "");
        if (!v.isEmpty()) {
            ret.insert(filter, v);
        }
    }
    return ret.toStringList();
}

} // namespace linglong::utils::command
