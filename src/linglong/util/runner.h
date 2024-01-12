/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_RUNNER_H_
#define LINGLONG_SRC_MODULE_UTIL_RUNNER_H_

#include "linglong/util/error.h"

#include <QDebug>
#include <QProcess>

#include <tuple>

namespace linglong {
namespace util {

/*!
 *
 * @param program
 * @param args
 * @param timeout: msecs
 * @return
 */
inline Error Exec(const QString &program,
                  const QStringList &args,
                  int timeout = -1,
                  QSharedPointer<QByteArray> standardOutput = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(args);
    QString standardError;

      QProcess::connect(&process, &QProcess::readyReadStandardOutput, [&]() {
          if (standardOutput) {
              standardOutput->append(process.readAllStandardOutput().trimmed());
          } else {
              qDebug() << QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
          }
      });

    QProcess::connect(&process, &QProcess::readyReadStandardError, [&]() {
        standardError.append(process.readAllStandardError().trimmed());
    });

    process.start();
    process.waitForStarted(timeout);
    process.waitForFinished(timeout);

    return NewError(process.exitCode(), standardError);
}

} // namespace util
} // namespace linglong
#endif
