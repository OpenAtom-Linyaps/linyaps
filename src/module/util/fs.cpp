/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fs.h"

#include <QDir>
#include <QStandardPaths>

namespace linglong {
namespace util {

QString jonsPath(const QStringList &component)
{
    return QDir::toNativeSeparators(component.join(QDir::separator()));
}

QString getUserFile(const QString &path)
{
    auto dirPath = QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0);
    if (!path.isEmpty()) {
        dirPath += "/" + path;
    }
    return dirPath;
}
QString ensureUserDir(const QString &relativeDirPath)
{
    QStringList dirPathComponents = {
        QStandardPaths::standardLocations(QStandardPaths::HomeLocation).at(0),
        relativeDirPath,
    };
    auto dirPath = QDir::cleanPath(dirPathComponents.join(QDir::separator()));
    QDir(dirPath).mkpath(".");
    return dirPath;
}

QString ensureUserDir(const QStringList &relativeDirPathComponents)
{
    auto relativeFilepath = QDir::cleanPath(relativeDirPathComponents.join(QDir::separator().toLatin1()));
    return ensureUserDir(relativeFilepath);
}

bool ensureDir(const QString &path)
{
    QDir dir(path);
    dir.mkpath(".");
    return true;
}

QString createProxySocket(const QString &pattern)
{
    auto userRuntimeDir = QString("/run/user/%1/").arg(getuid());
    QString socketDir = userRuntimeDir + ".dbus-proxy/";
    bool ret = util::createDir(socketDir);
    if (!ret) {
        qCritical() << "createProxySocket pattern:" << pattern << " failed";
        return "";
    }
    QTemporaryFile tmpFile(socketDir + pattern);
    tmpFile.setAutoRemove(false);
    if (!tmpFile.open()) {
        qCritical() << "create " << socketDir + pattern << " failed";
        return "";
    }
    tmpFile.close();
    return tmpFile.fileName();
}
} // namespace util
} // namespace linglong