/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <QMap>
#include <QString>
#include <QStringList>

namespace linglong::utils::command {

class Cmd
{
public:
    Cmd(const QString &command) noexcept;
    ~Cmd();

    virtual linglong::utils::error::Result<bool> exists() noexcept;
    virtual linglong::utils::error::Result<QString> exec() noexcept;
    virtual linglong::utils::error::Result<QString> exec(const QStringList &args) noexcept;
    virtual Cmd &setEnv(const QString &name, const QString &value) noexcept;

private:
    QString m_command;
    QMap<QString, QString> m_envs{};
};

} // namespace linglong::utils::command