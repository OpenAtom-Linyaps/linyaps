/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/package/package.h"
#include "module/util/singleton.h"
#include "package_manager_proxy_base.h"
#include "qdbus_retmsg.h"

using namespace linglong::service::util;

class PackageManagerFlatpakImpl
    : public PackageManagerProxyBase
    , public Single<PackageManagerFlatpakImpl>
{
public:
    RetMessageList Download(const QStringList &packageIDList, const QString &savePath) { return {}; }
    RetMessageList Install(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
    RetMessageList Uninstall(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
    AppMetaInfoList Query(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
};