/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "module/package/ref.h"

#include <QDebug>
#include <QStringList>

namespace linglong {
namespace package {

QString hostArch()
{
    return QSysInfo::buildCpuArchitecture();
}

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
        [[fallthrough]];
    case 3:
        arch = slice.value(2);
        [[fallthrough]];
    case 2:
        version = slice.value(1);
        [[fallthrough]];
    case 1:
        appId = slice.value(0);
        break;
    default:
        qCritical() << "invalid id" << id;
    }

    return Ref{ remoteRepo, channel, appId, version, arch, module };
}

Ref::Ref(const QString &id)
{
    *this = parseId(id);
}

} // namespace package
} // namespace linglong
