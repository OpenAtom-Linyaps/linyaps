/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

namespace linglong::utils::command {

class Cmd
{
public:
    Cmd(const QString &command) noexcept;
    ~Cmd();

    linglong::utils::error::Result<bool> exists() noexcept;
    linglong::utils::error::Result<QString> exec() noexcept;
    linglong::utils::error::Result<QString> exec(const QStringList &args) noexcept;

private:
    QString command;
};

} // namespace linglong::utils::command