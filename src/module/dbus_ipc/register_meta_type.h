/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_REGISTER_META_TYPE_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_REGISTER_META_TYPE_H_

#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/reply.h"
#include "module/package/package.h"
#include "module/util/serialize/json.h"

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
#endif
