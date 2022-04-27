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

#include <QJsonArray>

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
    linglong::service::Reply Install(const linglong::service::InstallParamOption &installParamOption);
    linglong::service::Reply Uninstall(const linglong::service::UninstallParamOption &paramOption);
    linglong::service::QueryReply Query(const linglong::service::QueryParamOption &paramOption);
};