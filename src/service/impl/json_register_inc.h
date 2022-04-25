/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "module/util/json.h"
#include "module/runtime/container.h"
#include "module/package/package.h"
#include "qdbus_retmsg.h"
#include "reply.h"
#include "param_option.h"
#include "package_manager_option.h"


//using linglong::service::PackageManagerOption;
//using linglong::service::PackageManagerOptionList;

namespace linglong {
namespace service {

inline void registerAllMetaType()
{
    qJsonRegister<RetMessage>();
    qDBusRegisterMetaType<linglong::service::Reply>();
    qDBusRegisterMetaType<linglong::service::ParamOption>();
    qDBusRegisterMetaType<linglong::service::DownloadParamOption>();
    qDBusRegisterMetaType<linglong::service::InstallParamOption>();
    qDBusRegisterMetaType<linglong::service::QueryParamOption>();
    qDBusRegisterMetaType<linglong::service::UninstallParamOption>();
    qDBusRegisterMetaType<ParamStringMap>();
    qJsonRegister<PackageManagerOption>();
    qJsonRegister<RunParamOption>();
}

} // namespace service
} // namespace linglong
