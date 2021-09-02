/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

} // namespace util
} // namespace linglong