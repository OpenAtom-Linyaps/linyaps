/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "filesystem_helper.h"

#include "linglong/dbus_ipc/dbus_common.h"
#include "linglong/system_helper/privilege/privilege_install_portal.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"

#include <QDir>

#include <sys/stat.h>

namespace linglong {
namespace system {
namespace helper {

static inline std::tuple<util::Error, QString> layerMountPoint(uid_t uid, const QString &layerID)
{
    auto parentDirPath = QString("/run/user/%1/linglong/").arg(uid);

    QDir dir(parentDirPath);
    dir.cd(layerID);
    auto mountPoint = dir.canonicalPath();

    // only allow umount in parentDirPath
    if (!mountPoint.startsWith(parentDirPath)) {
        return { NewError(-1, "Unsafe layerID to mount"), "" };
    }

    return { Success(), mountPoint };
}

static bool supportFscache()
{
    return util::fileExists("/dev/cachefiles");
}

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
 * mount file to sub path of "/run/user/{uid}/linglong/{layerID}"
 * @param layerID
 * @param options
 */
void FilesystemHelper::Mount(const QString &source,
                             const QString &layerID,
                             const QString &fsType,
                             const QVariantMap &options)
{
    if (fsType != "erofs") {
        sendErrorReply(QDBusError::InvalidArgs, "Unsupported fs type");
        return;
    }

    auto uid = getDBusCallerUid(*this);
    // !!! DO NOT create mount point because now is root
    auto [err, mountPoint] = layerMountPoint(uid, layerID);
    if (err) {
        sendErrorReply(QDBusError::InvalidArgs, err.message());
        return;
    }

    auto pid = getDBusCallerPid(*this);
    // FIXME: check if mount point is mount readonly
    if (!isWritable(mountPoint.toStdString().c_str(),
                    QString("/proc/%1").arg(pid).toStdString().c_str())) {
        sendErrorReply(QDBusError::AccessDenied, "No write permission with mount point");
        return;
    }

    if ((qEnvironmentVariable("LINGLONG_REPO_VFS_EROFS_BACKEND") == "fscache")
        && supportFscache()) {
        // mount -t erofs none -o fsid=${fsid} -o device=${blob} ${mountPoint}
        err = util::Exec("mount",
                         { "-t",
                           fsType,
                           "-o",
                           "loop",
                           "-o",
                           QString("fsid=%1").arg(options.value("fsid").toString()),
                           "-o",
                           QString("device=%1").arg(options.value("device").toString()),
                           "none",
                           mountPoint });
    } else {
        // mount -t erofs none -o loop ${source} ${mountPoint}
        err = util::Exec("mount", { "-t", fsType, "-o", "loop", source, mountPoint });
    }
    if (err) {
        sendErrorReply(static_cast<QDBusError::ErrorType>(err.code()), err.message());
    }
}

/*!
 * umount mount point in "/run/user/{uid}/linglong/{layerID}"
 * @param layerID
 * @param options
 */
void FilesystemHelper::Umount(const QString &layerID, const QVariantMap &options)
{
    auto uid = getDBusCallerUid(*this);
    auto [err, mountPoint] = layerMountPoint(uid, layerID);
    if (err) {
        sendErrorReply(QDBusError::InvalidArgs, err.message());
        return;
    }

    // no need to check mountPoint exist
    err = util::Exec("umount", { mountPoint });
    if (err) {
        sendErrorReply(QDBusError::Failed, err.message());
    }
}

} // namespace helper
} // namespace system
} // namespace linglong
