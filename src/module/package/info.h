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

namespace linglong {
namespace package {

const auto kDefaultRepo = "deepin";
const auto kDefaultChannel = "main";

class Ref
{
public:
    Ref(const QString &id);

    Ref(const QString &remote, const QString &id, const QString &version, const QString &arch)
        : repo(remote)
        , id(id)
        , version(version)
        , arch(arch)
    {
    }

    QString repo;
    QString channel;
    QString id;
    QString version;
    QString arch;
};

/*!
 * Info is the data of /opt/apps/{package-id}/info.json. The spec can get from here:
 * https://doc.chinauos.com/content/M7kCi3QB_uwzIp6HyF5J
 */
class Info : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Info)

public:
    Q_JSON_PROPERTY(QString, appid);
    Q_JSON_PROPERTY(QString, name);
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(QStringList, arch);

    // ref of runtime
    Q_JSON_PROPERTY(QString, runtime);
};

} // namespace package
} // namespace linglong

using linglong::package::Info;

Q_JSON_DECLARE_PTR_METATYPE(Info)

inline QDBusArgument &operator<<(QDBusArgument &argument, const Info &message)
{
    argument.beginStructure();
    argument << message.appid;
    argument << message.name;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Info &message)
{
    argument.beginStructure();
    argument >> message.appid;
    argument >> message.name;
    argument.endStructure();
    return argument;
}
