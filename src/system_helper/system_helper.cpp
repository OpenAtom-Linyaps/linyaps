/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "system_helper.h"

#include "privilege/privilege_install_portal.h"

namespace linglong {
namespace system {
namespace helper {

/*!
 * RebuildInstallPortal update package file need portal to host when package install
 * @param installPath the package install path, without "files"
 * @param ref not use now
 * @param options not use now
 */
void SystemHelper::RebuildInstallPortal(const QString &installPath, const QString &ref, const QVariantMap &options)
{
    qDebug() << "call PostInstall" << installPath << ref << options;
    auto err = rebuildPrivilegeInstallPortal(installPath, ref, options);
    if (!err.success()) {
        sendErrorReply(static_cast<QDBusError::ErrorType>(err.code()), err.message());
    }
}

/*!
 * WARN: call RuinInstallPortal before remove files from installPath. it will remove portal link to host,
 * @param installPath
 * @param ref
 * @param options
 */
void SystemHelper::RuinInstallPortal(const QString &installPath, const QString &ref, const QVariantMap &options)
{
    auto err = ruinPrivilegeInstallPortal(installPath, ref, options);
    if (!err.success()) {
        sendErrorReply(static_cast<QDBusError::ErrorType>(err.code()), err.message());
    }
}

} // namespace helper
} // namespace system
} // namespace linglong