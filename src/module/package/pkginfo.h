/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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
