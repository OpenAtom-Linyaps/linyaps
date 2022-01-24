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

#include "module/package/package.h"
#include "oci.h"

class Container : public JsonSerialize
{
    Q_OBJECT
    Q_JSON_CONSTRUCTOR(Container)
public:
    Q_JSON_PROPERTY(QString, id)
    Q_JSON_PROPERTY(qint64, pid)
    Q_JSON_PROPERTY(QString, workingDirectory)
};
Q_JSON_DECLARE_PTR_METATYPE(Container)

inline QDBusArgument &operator<<(QDBusArgument &argument,
                                 const Container &message)
{
    argument.beginStructure();
    argument << message.id;
    argument << message.pid;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument,
                                       Container &message)
{
    argument.beginStructure();
    argument >> message.id;
    argument >> message.pid;
    argument.endStructure();
    return argument;
}
