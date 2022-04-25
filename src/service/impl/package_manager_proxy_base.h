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
#include "reply.h"
#include "param_option.h"
#include "module/package/ref.h"
#include "module/util/result.h"

class PackageManagerProxyBase
{
public:
    virtual linglong::service::Reply Download(const linglong::service::DownloadParamOption &downloadParamOption) = 0;
    virtual linglong::service::Reply Install(const linglong::service::InstallParamOption &installParamOption) = 0;
    virtual linglong::service::Reply Uninstall(const linglong::service::UninstallParamOption &paramOption) = 0;
    virtual linglong::package::AppMetaInfoList Query(const linglong::service::QueryParamOption &paramOption) = 0;

    // TODO: move PackageManagerProxyBase to namespace
    // TODO: need pure virtual
    virtual linglong::service::Reply Update(const linglong::service::ParamOption &paramOption) { return {}; }
};
