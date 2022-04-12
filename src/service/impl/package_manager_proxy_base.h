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
#include "module/package/ref.h"
#include "module/util/result.h"

class PackageManagerProxyBase
{
public:
    virtual RetMessageList Download(const QStringList &packageIDList, const QString &savePath) = 0;
    virtual RetMessageList Install(const QStringList &packageIDList, const ParamStringMap &paramMap = {}) = 0;
    virtual RetMessageList Uninstall(const QStringList &packageIDList, const ParamStringMap &paramMap = {}) = 0;
    virtual AppMetaInfoList Query(const QStringList &packageIDList, const ParamStringMap &paramMap = {}) = 0;

    // TODO: move PackageManagerProxyBase to namespace
    // TODO: need pure virtual
    virtual RetMessageList Update(const QStringList &packageIDList, const ParamStringMap &paramMap = {}) { return {}; }
};
