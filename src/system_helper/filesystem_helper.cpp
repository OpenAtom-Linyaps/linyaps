/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "filesystem_helper.h"

#include "module/dbus_ipc/dbus_common.h"
#include "module/util/runner.h"
#include "privilege/privilege_install_portal.h"

#include <QDir>

#include <sys/stat.h>

namespace linglong {
namespace system {
namespace helper {

bool isWritable(const char *path, const char *procPath)
{
    struct stat buf = {};

    stat(procPath, &buf);
    uid_t procUid = buf.st_uid;
    gid_t procGid = buf.st_gid;

    stat(path, &buf);
    if (procUid == buf.st_uid && buf.st_mode & S_IWUSR) {
        return true;
    } else if (procGid == buf.st_gid && buf.st_mode & S_IWGRP) {
        return true;
    } else if (buf.st_mode & S_IWOTH) {
        return true;
    }
    return false;
}

/*!
 * mount file to sub path of "/run/user/{uid}/linglong"
 * @param mountPoint
 * @param options
 */
void FilesystemHelper::Mount(const QString &source,
                             const QString &mountPoint,
                             const QString &fsType,
                             const QVariantMap &options)
{
    if (fsType != "erofs") {
        sendErrorReply(QDBusError::InvalidArgs, "Unsupported fs type");
        return;
    }

    auto pid = getDBusCallerPid(*this);
    // FIXME: check if mountpoint is mount readonly
    if (!isWritable(mountPoint.toStdString().c_str(),
                    QString("/proc/%1").arg(pid).toStdString().c_str())) {
        sendErrorReply(QDBusError::AccessDenied, "No write permission with mount point");
        return;
    }

    auto err = util::Exec("mount", { "-t", fsType, "-o", "loop", source, mountPoint });
    if (err) {
        sendErrorReply(static_cast<QDBusError::ErrorType>(err.code()), err.message());
    }
}

/*!
 * umount mountpoint in "/run/user/{uid}/linglong"
 * @param mountPoint
 * @param options
 */
void FilesystemHelper::Umount(const QString &mountPoint, const QVariantMap &options)
{
    auto uid = getDBusCallerUid(*this);
    auto mountPointDir = QDir(mountPoint);
    auto allowMountPointPathPrefix = QString("/run/user/%1/linglong/").arg(uid);

    // only allow umount in "/run/user/{uid}/linglong"
    if (!mountPointDir.canonicalPath().startsWith(allowMountPointPathPrefix)) {
        sendErrorReply(QDBusError::AccessDenied, "No allow umount mountpoint not belong linglong");
        return;
    }

    auto err = util::Exec("umount", { mountPoint });
    if (err) {
        sendErrorReply(static_cast<QDBusError::ErrorType>(err.code()), err.message());
    }
}

} // namespace helper
} // namespace system
} // namespace linglong
