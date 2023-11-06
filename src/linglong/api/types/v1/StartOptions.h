/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_API_V1_TYPES_STARTOPTIONS_H_
#define LINGLONG_API_V1_TYPES_STARTOPTIONS_H_

#include "linglong/api/types/v1/CommonOptions.h"
#include "linglong/api/types/v1/DBusProxyOptions.h"

#include <qserializer/dbus.h>

#include <QString>

namespace linglong::api::v1::types {

class StartOptions : public CommonOptions
{
    Q_GADGET;

public:
    Q_PROPERTY(QList<QString> command MEMBER command);
    QList<QString> command;

    Q_PROPERTY(QList<QString> env MEMBER env);
    QList<QString> env;

    Q_PROPERTY(QString cwd MEMBER cwd);
    QString cwd;

    Q_PROPERTY(QSharedPointer<DBusProxyOptions> dbusProxy MEMBER dbusProxy);
    QSharedPointer<DBusProxyOptions> dbusProxy;
};

} // namespace linglong::api::v1::types

QSERIALIZER_DECLARE_DBUS(linglong::api::v1::types::StartOptions);

#endif
