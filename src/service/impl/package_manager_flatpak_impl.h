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

#include "module/util/singleton.h"
#include "module/package/package.h"
#include "package_manager_proxy_base.h"
#include "qdbus_retmsg.h"
#include "reply.h"
#include "param_option.h"

class PackageManagerFlatpakImpl
    : public PackageManagerProxyBase
    , public linglong::util::Singleton<PackageManagerFlatpakImpl>
{
public:
    linglong::service::Reply Download(const linglong::service::DownloadParamOption &downloadParamOption) { return linglong::service::Reply(); }
    RetMessageList Install(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
    RetMessageList Uninstall(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
    linglong::package::AppMetaInfoList Query(const QStringList &packageIDList, const ParamStringMap &paramMap = {});
};