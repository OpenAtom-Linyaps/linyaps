/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "info.h"

namespace linglong {
namespace package {


QString hostArch()
{
    return QSysInfo::buildCpuArchitecture();
}

/*!
 * deepin/channel:appID/version/arch -> appID/version -> appID
 * \param ref string
 */
Ref parseID(const QString &id)
{
    QStringList slice = id.split(":");
    QString remoteRepo = kDefaultRepo;
    QString localID = id;
    switch (slice.length()) {
    case 2:
        remoteRepo = slice.value(0);
        localID = slice.value(1);
        break;
    case 1:
        localID = slice.value(0);
        break;
    default:
        qCritical() << "invalid id" << id;
    }

    slice = localID.split("/");

    QString appID = localID;
    QString version = "latest";
    QString arch = hostArch();

    switch (slice.length()) {
    case 3:
        arch = slice.value(2);
    case 2:
        version = slice.value(1);
    case 1:
        appID = slice.value(0);
        break;
    default:
        qCritical() << "invalid id" << id;
    }
    return Ref {remoteRepo, appID, version, arch};
}

Ref::Ref(const QString &id)
{
    *this = parseID(id);
}

} // namespace package
} // namespace linglong