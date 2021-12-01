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

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>
#include <QDBusArgument>
#include <QObject>
#include <QList>
#include <QFile>
#include <QDebug>
#include <QTemporaryDir>
#include <QFileInfo>

#include "module/util/fs.h"
#include "module/util/repo.h"
#include "service/impl/dbus_retcode.h"

using linglong::dbus::RetCode;

class Package
{
public:
    QString ID;
    QString name;
    QString configJson;
    QString dataDir;
    QString dataPath;
};

typedef QList<Package> PackageList;

Q_DECLARE_METATYPE(Package)

Q_DECLARE_METATYPE(PackageList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const Package &message)
{
    argument.beginStructure();
    argument << message.ID;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, Package &message)
{
    argument.beginStructure();
    argument >> message.ID;
    argument.endStructure();

    return argument;
}

typedef QMap<QString,QString> ParamStringMap;
Q_DECLARE_METATYPE(ParamStringMap)