/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
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

#pragma once

#include <QDBusArgument>
#include <QObject>
#include <QList>
#include <string>

#include "module/uab/uap.h"
#include "module/util/fs.h"

using format::uap::UAP;
using linglong::util::fileExists;
using linglong::util::dirExists;

class Package {
 public:
    QString ID;
    QString name;
    QString appName;
    QString configJson;
    QString origData;

 protected:
    UAP *uap;

 public:
    // TODO(RD): 创建package
    bool Init(const QString config) {
        if (!fileExists(config)) { return false; }
        this->configJson = config;
        return true;
    }

    // TODO(RD): 创建package的数据包
    bool InitData(const QString data) {
        if (!dirExists(data)) { return false; }
        this->origData = data;
        return true;
    }
};

typedef QList<Package> PackageList;

Q_DECLARE_METATYPE(Package)

Q_DECLARE_METATYPE(PackageList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const Package &message) {
    argument.beginStructure();
    argument << message.ID;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Package &message) {
    argument.beginStructure();
    argument >> message.ID;
    argument.endStructure();

    return argument;
}
