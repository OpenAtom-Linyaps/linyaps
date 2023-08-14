/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "erofs.h"

#include "linglong/api/v1/dbus/filesystem_helper1.h"
#include "linglong/dbus_ipc/dbus_system_helper_common.h"
#include "runner.h"

namespace linglong {
namespace erofs {

util::Error mount(const QString &src, const QString &mountPoint)
{
    // TODO: check by config, not env
    if (qEnvironmentVariable("LINGLONG_REPO_VFS_EROFS_BACKEND") == "fuse") {
        return util::Exec("erofsfuse", { src, mountPoint });
    }

    api::v1::dbus::PackageManagerHelper1 ifc(SystemHelperDBusServiceName,
                                                        FilesystemHelperDBusPath,
                                                        QDBusConnection::systemBus());

    QVariantMap option = {};
    if (qEnvironmentVariable("LINGLONG_REPO_VFS_EROFS_BACKEND") == "fscache") {
        // TODO(Iceyer): design the format of fscache, or use nydusd
        option = {
            { "fsid", "" },
            { "device", "" },
        };
    }

    auto reply = ifc.Mount(src, mountPoint, "erofs", {});
    // FIXME: add dbus error convert
    return NewError(reply.error().type(), reply.error().message());
}

util::Error mkfs(const QString &srcDir, const QString &destImagePath)
{
    return util::Exec("mkfs.erofs", { "-zlz4", destImagePath, srcDir });
}

} // namespace erofs
} // namespace linglong
