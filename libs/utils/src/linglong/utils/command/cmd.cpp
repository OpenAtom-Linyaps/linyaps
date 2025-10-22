/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cmd.h"

#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#include <QProcess>
#include <QStandardPaths>

#include <utility>

namespace linglong::utils::command {

Cmd::Cmd(QString command) noexcept
    : m_command(std::move(command))
{
}

Cmd::~Cmd() = default;

linglong::utils::error::Result<QString> Cmd::exec() noexcept
{
    return exec({});
}

linglong::utils::error::Result<QString> Cmd::exec(const QStringList &args) noexcept
{
    LINGLONG_TRACE(
      fmt::format("exec {} {}", m_command.toStdString(), args.join(" ").toStdString()));
    LogD("exec {} {}", m_command, args);
    QProcess process;
    process.setProgram(m_command);
    if (!m_envs.isEmpty()) {
        // Merge system and custom environment variables
        auto envs = QProcessEnvironment::systemEnvironment();
        for (auto it = m_envs.cbegin(); it != m_envs.cend(); ++it) {
            const auto &key = it.key();
            const auto &value = it.value();
            if (value.isEmpty()) {
                envs.remove(key);
            } else {
                envs.insert(key, value);
            }
        }
        process.setProcessEnvironment(envs);
    }
    process.setArguments(args);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForFinished(-1)) {
        return LINGLONG_ERR(process.errorString(), process.error());
    }

    if (process.exitCode() != 0) {
        return LINGLONG_ERR(process.readAllStandardOutput().toStdString(), process.exitCode());
    }

    return process.readAllStandardOutput();
}

bool Cmd::exists() noexcept
{
    auto ret = QStandardPaths::findExecutable(m_command);
    return !ret.isEmpty();
}

// set environment, if value is empty, remove the environment
Cmd &Cmd::setEnv(const QString &name, const QString &value) noexcept
{
    m_envs[name] = value;
    return *this;
}

} // namespace linglong::utils::command
