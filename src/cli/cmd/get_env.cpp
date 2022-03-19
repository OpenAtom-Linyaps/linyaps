/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:    liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdlib.h>

#include <QStringList>
#include <QtGlobal>

#include "cmd.h"

QStringList getUserEnv(const QStringList &envList)
{
    QStringList userEnvList;
    auto bypassEnv = [&](const char *constEnv) {
        if (qEnvironmentVariableIsSet(constEnv)) {
            userEnvList.append(QString(constEnv) + "=" + QString(getenv(constEnv)));
        }
    };

    for (auto &env : envList) {
        bypassEnv(env.toStdString().c_str());
    }
    return userEnvList;
}