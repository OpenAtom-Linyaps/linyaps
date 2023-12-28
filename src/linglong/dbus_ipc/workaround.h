/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_WORKAROUND_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_WORKAROUND_H_

#include "package_manager_param.h"
#include "param_option.h"
#include "reply.h"

#include <QDBusMetaType>

inline void registerDBusParam()
{
    qDBusRegisterMetaType<linglong::service::ExecParamOption>();
    qDBusRegisterMetaType<linglong::service::InstallParamOption>();
    qDBusRegisterMetaType<linglong::service::ParamOption>();
    qDBusRegisterMetaType<linglong::service::QueryParamOption>();
    qDBusRegisterMetaType<linglong::service::QueryReply>();
    qDBusRegisterMetaType<linglong::service::Reply>();
    qDBusRegisterMetaType<linglong::service::RunParamOption>();
    qDBusRegisterMetaType<linglong::service::UninstallParamOption>();
}

#endif
