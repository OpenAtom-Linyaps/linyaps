/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_

#include "module/util/sysinfo.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusReply>
#include <QDebug>
#include <QVariantMap>

typedef QList<QVariantMap> QVariantMapList;

Q_DECLARE_METATYPE(QVariantMapList);

inline QVariant toVariant(const QVariantMapList &vml)
{
    QVariantList vl;
    for (auto const &pkg : vml) {
        vl.push_back(pkg);
    }
    return vl;
}

namespace linglong {

inline bool registerServiceAndObject(QDBusConnection *dbus,
                                     const QString &serviceName,
                                     QMap<QString, QObject *> objects)
{
    if (!serviceName.isEmpty()) {
        if (!dbus->registerService(serviceName)) {
            qCritical() << "registerService" << serviceName << "failed" << dbus->lastError();
            return false;
        }
    }

    for (auto const &objPath : objects.keys()) {
        if (!dbus->registerObject(objPath, objects[objPath])) {
            qCritical() << "registerObject" << objPath << "failed" << dbus->lastError();
            return false;
        }
    }

    return true;
}

inline QString getDBusCallerUsername(QDBusContext &ctx)
{
    QDBusReply<uint> dbusReply = ctx.connection().interface()->serviceUid(ctx.message().service());
    if (dbusReply.isValid()) {
        return util::getUserName(dbusReply.value());
    }
    // FIXME: must reject request
    qCritical() << "can not find caller uid";
    return "";
}

inline uid_t getDBusCallerUid(QDBusContext &ctx)
{
    if (!ctx.connection().interface()) {
        qCritical() << "can not get peer uid";
        return -1;
    }

    QDBusReply<uint> dbusReply = ctx.connection().interface()->serviceUid(ctx.message().service());
    if (!dbusReply.isValid()) {
        qCritical() << "can not find caller uid";
        return -1;
    }
    return dbusReply.value();
}

inline pid_t getDBusCallerPid(QDBusContext &ctx)
{
    if (!ctx.connection().interface()) {
        qCritical() << "can not get peer uid";
        return -1;
    }

    QDBusReply<uint> dbusReply = ctx.connection().interface()->servicePid(ctx.message().service());
    if (!dbusReply.isValid()) {
        qCritical() << "can not find caller uid";
        return -1;
    }
    return dbusReply.value();
}

} // namespace linglong

#endif // LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_
