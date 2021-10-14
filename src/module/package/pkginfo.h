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
#include <QList>
#include <QObject>
#include "module/util/json.h"

/*!
 * PKGInfo
 * 查询结果包含软件包提供商、证书、名称、版本、支持架构、大小、来源、描述信息等。
 */
class PKGInfo : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(PKGInfo)

public:
    Q_JSON_PROPERTY(QString, appid);
    Q_JSON_PROPERTY(QString, appname);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QString, arch);
    Q_JSON_PROPERTY(QString, description);
    // PkgExt

    // Permission
    // Owner
};
Q_JSON_DECLARE_PTR_METATYPE(PKGInfo)

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const PKGInfo &message)
{
    argument.beginStructure();
    argument << message.appid;
    argument << message.appname;
    argument << message.version;
    argument << message.arch;
    argument << message.description;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       PKGInfo &message)
{
    argument.beginStructure();
    argument >> message.appid;
    argument >> message.appname;
    argument >> message.version;
    argument >> message.arch;
    argument >> message.description;
    argument.endStructure();
    return argument;
}
