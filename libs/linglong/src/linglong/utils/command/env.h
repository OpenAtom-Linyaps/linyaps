/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_COMMAND_ENV_H_
#define LINGLONG_UTILS_COMMAND_ENV_H_

#include "linglong/utils/error/error.h"

#include <QStringList>

namespace linglong::utils::command {

error::Result<QString> Exec(QString command, QStringList args);
QStringList getUserEnv(const QStringList &filters);
extern const QStringList envList;

} // namespace linglong::utils::command

#endif
