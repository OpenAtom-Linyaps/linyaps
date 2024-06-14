/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_DBUS_H_
#define LINGLONG_UTILS_DBUS_H_

#include "linglong/utils/dbus/log.h"
#include "linglong/utils/error/error.h"
#include "tl/expected.hpp"

#include <QDBusConnection>
#include <QDBusError>

namespace linglong::utils::dbus {

[[nodiscard]] inline auto registerDBusObject(QDBusConnection conn,
                                             const QString &path,
                                             QObject *obj) -> tl::expected<void, error::Error>
{
    LINGLONG_TRACE("register D-Bus object");
    if (conn.registerObject(path, obj)) {
        qCDebug(linglong_utils_dbus) << "register object to dbus on" << path;
        return LINGLONG_OK;
    }

    return LINGLONG_ERR("object existed.");
}

inline void unregisterDBusObject(QDBusConnection conn, const QString &path)
{
    conn.unregisterObject(path);
    qCDebug(linglong_utils_dbus) << "unregister object to dbus on" << path;
}

[[nodiscard]] inline auto registerDBusService(QDBusConnection conn,
                                              const QString &serviceName) -> error::Result<void>
{
    LINGLONG_TRACE("register D-Bus service");

    if (conn.registerService(serviceName)) {
        qCDebug(linglong_utils_dbus) << "register dbus name" << serviceName;
        return LINGLONG_OK;
    }

    error::Error err;
    // FIXME: use real ERROR CODE defined in API.
    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(conn.lastError().name() + ": " + conn.lastError().message());
    }

    return LINGLONG_ERR("service name existed.");
}

[[nodiscard]] inline auto unregisterDBusService(QDBusConnection conn, const QString &serviceName)
  -> tl::expected<void, error::Error>
{
    LINGLONG_TRACE("unregister D-Bus service");

    if (conn.unregisterService(serviceName)) {
        qCDebug(linglong_utils_dbus) << "unregister dbus name" << serviceName;
        return LINGLONG_OK;
    }

    // FIXME: use real ERROR CODE defined in API.
    error::Error err;
    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(conn.lastError().name() + ": " + conn.lastError().message());
    }

    return LINGLONG_ERR("unknown");
}

} // namespace linglong::utils::dbus

#endif
