/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>

#include "qdbus_retmsg.h"

class PackageManagerProxyBase
{
public:
    virtual RetMessageList Download(const QStringList &packageIDList, const QString &savePath) = 0;
    virtual RetMessageList Install(const QStringList &packageIDList) = 0;
    virtual QString Uninstall(const QStringList &packageIDList) = 0;
    virtual PKGInfoList Query(const QStringList &packageIDList) = 0;
};
