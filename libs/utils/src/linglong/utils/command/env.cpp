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

linglong::utils::error::Result<QString> Exec(const QString &command,
                                             const QStringList &args) noexcept
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

} // namespace linglong::utils::command
