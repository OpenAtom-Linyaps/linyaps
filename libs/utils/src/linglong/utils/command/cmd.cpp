/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cmd.h"

#include <QProcess>

namespace linglong::utils::command {

Cmd::Cmd(const QString &command) noexcept
{
    this->m_command = command;
}

Cmd::~Cmd() { }

linglong::utils::error::Result<QString> Cmd::exec() noexcept
{
    return exec({});
}

linglong::utils::error::Result<QString> Cmd::exec(const QStringList &args) noexcept
{
    LINGLONG_TRACE(QString("exec %1 %2").arg(m_command, args.join(" ")));
    qDebug() << "exec" << m_command << args;
    QProcess process;
    process.setProgram(m_command);
    if (!m_envs.isEmpty()) {
        // Merge system and custom environment variables
        QStringList envs = QProcess::systemEnvironment();
        for (const auto &key : m_envs.keys()) {
            envs.push_back(key + "=" + m_envs[key]);
        }
        process.setEnvironment(envs);
    }
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
    LINGLONG_TRACE(QString("check command %1 exists").arg(m_command));
    qDebug() << "exists" << m_command;
    QProcess process;
    process.setProgram("sh");
    if (!m_envs.isEmpty()) {
        // Merge system and custom environment variables
        QStringList envs = QProcess::systemEnvironment();
        for (const auto &key : m_envs.keys()) {
            envs.push_back(key + "=" + m_envs[key]);
        }
        process.setEnvironment(envs);
    }
    process.setArguments({ "-c", QString("command -v %1").arg(m_command) });
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(-1)) {
        return LINGLONG_ERR(process.errorString(), process.error());
    }
    return process.exitCode() == 0;
}

// set environment, if value is empty, remove the environment
Cmd &Cmd::setEnv(const QString &name, const QString &value) noexcept
{
    if (value.isEmpty()) {
        m_envs.remove(name);
        return *this;
    }
    m_envs[name] = value;
    return *this;
}

} // namespace linglong::utils::command
