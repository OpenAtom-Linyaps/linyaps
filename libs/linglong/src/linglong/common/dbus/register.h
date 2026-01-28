/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/common/formatter.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <QDBusConnection>
#include <QDBusError>

namespace linglong::common::dbus {

[[nodiscard]] inline auto registerDBusObject(QDBusConnection conn,
                                             const QString &path,
                                             QObject *obj) -> utils::error::Result<void>
{
    LINGLONG_TRACE("register D-Bus object");
    if (conn.registerObject(path, obj)) {
        LogD("register object to dbus on {}", path);
        return LINGLONG_OK;
    }

    return LINGLONG_ERR("object existed.");
}

inline void unregisterDBusObject(QDBusConnection conn, const QString &path)
{
    conn.unregisterObject(path);
    LogD("unregister object to dbus on {}", path);
}

[[nodiscard]] inline auto registerDBusService(QDBusConnection conn, const QString &serviceName)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("register D-Bus service");

    if (conn.registerService(serviceName)) {
        LogD("register dbus name {}", serviceName);
        return LINGLONG_OK;
    }

    // FIXME: use real ERROR CODE defined in API.
    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(
          fmt::format("{}: {}", conn.lastError().name(), conn.lastError().message()));
    }

    return LINGLONG_ERR("service name existed.");
}

[[nodiscard]] inline auto unregisterDBusService(QDBusConnection conn, const QString &serviceName)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("unregister D-Bus service");

    if (conn.unregisterService(serviceName)) {
        LogD("unregister dbus name {}", serviceName);
        return LINGLONG_OK;
    }

    // FIXME: use real ERROR CODE defined in API.
    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(
          fmt::format("{}: {}", conn.lastError().name(), conn.lastError().message()));
    }

    return LINGLONG_ERR("unknown");
}

} // namespace linglong::common::dbus
