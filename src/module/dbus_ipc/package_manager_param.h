/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_PACKAGE_MANAGER_PARAM_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_PACKAGE_MANAGER_PARAM_H_

#include <QString>

namespace linglong {
namespace util {
const QString kKeyVersion = "version";
const QString kKeyRepoPoint = "repo-point";
const QString kKeyNoCache = "force";
const QString kKeyExec = "exec";
const QString kKeyEnvlist = "env-list";
const QString kKeyBusType = "bus-type";
const QString kKeyNoProxy = "no-proxy";
const QString kKeyFilterName = "filter-name";
const QString kKeyFilterPath = "filter-path";
const QString kKeyFilterIface = "filter-interface";

const QString kKeyDelData = "delete-data";
} // namespace util
} // namespace linglong
#endif
