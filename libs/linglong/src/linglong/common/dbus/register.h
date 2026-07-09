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

    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(
          fmt::format("failed to register D-Bus service '{}': {}", serviceName, conn.lastError().message()),
          utils::error::ErrorCode::Failed);
    }

    return LINGLONG_ERR(
      fmt::format("D-Bus service name '{}' already registered", serviceName),
      utils::error::ErrorCode::Failed);
}

[[nodiscard]] inline auto unregisterDBusService(QDBusConnection conn, const QString &serviceName)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("unregister D-Bus service");

    if (conn.unregisterService(serviceName)) {
        LogD("unregister dbus name {}", serviceName);
        return LINGLONG_OK;
    }

    if (conn.lastError().isValid()) {
        return LINGLONG_ERR(
          fmt::format("failed to unregister D-Bus service '{}': {}", serviceName, conn.lastError().message()),
          utils::error::ErrorCode::Failed);
    }

    return LINGLONG_ERR(
      fmt::format("D-Bus service name '{}' not found", serviceName),
      utils::error::ErrorCode::Failed);
}

} // namespace linglong::common::dbus
