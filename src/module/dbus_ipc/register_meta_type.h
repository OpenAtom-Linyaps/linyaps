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
#include "module/dbus_ipc/reply.h"
#include "module/dbus_ipc/param_option.h"
#include "module/package/package.h"

namespace linglong {
namespace service {

inline void registerAllMetaType()
{
    qDBusRegisterMetaType<linglong::service::Reply>();
    qDBusRegisterMetaType<linglong::service::QueryReply>();
    qDBusRegisterMetaType<linglong::service::ParamOption>();
    qDBusRegisterMetaType<linglong::service::DownloadParamOption>();
    qDBusRegisterMetaType<linglong::service::InstallParamOption>();
    qDBusRegisterMetaType<linglong::service::QueryParamOption>();
    qDBusRegisterMetaType<linglong::service::UninstallParamOption>();
    qDBusRegisterMetaType<ParamStringMap>();
    qDBusRegisterMetaType<linglong::service::RunParamOption>();
    qDBusRegisterMetaType<linglong::service::ExecParamOption>();
}

} // namespace service
} // namespace linglong
