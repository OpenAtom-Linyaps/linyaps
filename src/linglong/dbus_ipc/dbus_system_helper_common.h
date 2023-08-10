/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_

namespace linglong {

inline const char *SystemHelperDBusServiceName = "org.deepin.linglong.SystemHelper";

inline const char *PackageManagerHelperDBusPath = "/org/deepin/linglong/PackageManagerHelper";
inline const char *PackageManagerHelperDBusInterface = "org.deepin.linglong.PackageManagerHelper";

inline const char *FilesystemHelperDBusPath = "/org/deepin/linglong/FilesystemHelper";
inline const char *FilesystemHelperDBusInterface = "org.deepin.linglong.FilesystemHelper";

} // namespace linglong

#endif // LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_SYSTEM_HELPER_COMMON_H_
