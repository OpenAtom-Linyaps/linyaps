/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_
#define LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_

#include <QDBusConnection>
#include <QDBusError>
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

} // namespace linglong

#endif // LINGLONG_SRC_MODULE_DBUS_IPC_DBUS_COMMON_H_
