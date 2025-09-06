/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cmd.h"

#include "linglong/utils/bash_quote.h"

#include <QProcess>

namespace linglong::utils::command {

Cmd::Cmd(const QString &command) noexcept
{
    this->command = command;
}

Cmd::~Cmd() { }

linglong::utils::error::Result<QString> Cmd::exec() noexcept
{
    return exec({});
}

linglong::utils::error::Result<QString> Cmd::exec(const QStringList &args) noexcept
{
    LINGLONG_TRACE(QString("exec %1 %2").arg(command, args.join(" ")));
    qDebug() << "exec" << command << args;
    QProcess process;
    process.setProgram(command);
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForFinished(-1)) {
        return LINGLONG_ERR(process.errorString(), process.error());
    }

    if (process.exitCode() != 0) {
        return LINGLONG_ERR(process.readAllStandardOutput(), process.exitCode());
    }

    return process.readAllStandardOutput();
}

linglong::utils::error::Result<bool> Cmd::exists() noexcept
{
    LINGLONG_TRACE(QString("check command %1 exists").arg(command));
    qDebug() << "exists" << command;
    QProcess process;
    process.setProgram("sh");

    // Use proper shell escaping to prevent command injection
    auto quotedCommand = QString::fromStdString(quoteBashArg(command.toStdString()));
    process.setArguments({ "-c", QString("command -v %1").arg(quotedCommand) });

    process.start();
    if (!process.waitForFinished(-1)) {
        return LINGLONG_ERR(process.errorString(), process.error());
    }
    return process.exitCode() == 0;
}

} // namespace linglong::utils::command