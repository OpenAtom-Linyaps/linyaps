/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_DBUS_H_
#define LINGLONG_UTILS_DBUS_H_

#include "QDBusConnection"
#include "QDBusError"
#include "linglong/utils/error/error.h"
#include "tl/expected.hpp"

namespace linglong::utils::dbus {

[[nodiscard]] auto registerDBusObject(QDBusConnection conn, const QString &path, QObject *obj)
        -> tl::expected<void, error::Error>
{
    if (conn.registerObject(path, obj)) {
        return Ok;
    }

    // FIXME: use real ERROR CODE defined in API.
    return tl::unexpected(Err(-1, "register dbus object: object existed."));
}

inline void unregisterDBusObject(QDBusConnection conn, const QString &path)

{
    conn.unregisterObject(path);
}

[[nodiscard]] inline auto registerDBusService(QDBusConnection conn, const QString &serviceName)
        -> tl::expected<void, error::Error>
{
    if (conn.registerService(serviceName)) {
        return Ok;
    }

    // FIXME: use real ERROR CODE defined in API.
    auto err = Err(-1, QString("%1: %2").arg(conn.lastError().name(), conn.lastError().message()));
    return tl::unexpected(EWrap("register dbus service:", err));
}

[[nodiscard]] inline auto unregisterDBusService(QDBusConnection conn, const QString &serviceName)
        -> tl::expected<void, error::Error>
{
    if (conn.unregisterService(serviceName)) {
        return Ok;
    }

    // FIXME: use real ERROR CODE defined in API.
    auto err = Err(-1, QString("%1: %2").arg(conn.lastError().name(), conn.lastError().message()));
    return tl::unexpected(EWrap("register dbus service:", err));
}

} // namespace linglong::utils::dbus

#endif
