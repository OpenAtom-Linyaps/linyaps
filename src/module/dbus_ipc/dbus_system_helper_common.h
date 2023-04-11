/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_

namespace linglong {

// FIXME(black_desk):
// Should not define var in header. Should we use macro here?

const char *SystemHelperDBusServiceName = "org.deepin.linglong.SystemHelper";

const char *SystemHelperDBusPath = "/org/deepin/linglong/SystemHelper";
const char *SystemHelperDBusInterface = "org.deepin.linglong.SystemHelper";

const char *FilesystemHelperDBusPath = "/org/deepin/linglong/FilesystemHelper";
const char *FilesystemHelperDBusInterface = "org.deepin.linglong.FilesystemHelper";

} // namespace linglong

#endif // LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_
