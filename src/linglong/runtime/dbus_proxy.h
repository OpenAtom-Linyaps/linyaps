/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_RUNTIME_DBUS_PROXY_H_
#define LINGLONG_RUNTIME_DBUS_PROXY_H_

#include <QString>
#include <QStringList>

class DBusProxyConfig
{
public:
    bool enable;
    QString busType;
    QString appId;
    QString proxyPath;
    QStringList name;
    QStringList path;
    QStringList interface;
};

#endif
