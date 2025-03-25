/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <QStringList>

namespace linglong::utils::command {

linglong::utils::error::Result<QString> Exec(QString command, QStringList args);
QStringList getUserEnv(const QStringList &filters);
extern const QStringList envList;

} // namespace linglong::utils::command
