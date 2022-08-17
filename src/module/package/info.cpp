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
 * deepin/channel:appId/version/arch -> appId/version -> appId
 * \param ref string
 */
Ref parseId(const QString &id)
{
    QStringList slice = id.split(":");
    QString remoteRepo = "";
    QString localId = id;
    switch (slice.length()) {
    case 2:
        remoteRepo = slice.value(0);
        localId = slice.value(1);
        break;
    case 1:
        localId = slice.value(0);
        break;
    default:
        qCritical() << "invalid id" << id;
    }

    slice = localId.split("/");

    QString appId = localId;
    QString version = "latest";
    QString arch = hostArch();

    QString channel = "linglong";
    QString module = "runtime";

    switch (slice.length()) {
    case 5:
        channel = slice.value(0);
        appId = slice.value(1);
        version = slice.value(2);
        arch = slice.value(3);
        module = slice.value(4);
        break;
    case 4:
        module = slice.value(3);
    case 3:
        arch = slice.value(2);
    case 2:
        version = slice.value(1);
    case 1:
        appId = slice.value(0);
        break;
    default:
        qCritical() << "invalid id" << id;
    }

    return Ref {remoteRepo, channel, appId, version, arch, module};
}

Ref::Ref(const QString &id)
{
    *this = parseId(id);
}

} // namespace package
} // namespace linglong