/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_UTILS_DBUS_H_
#define LINGLONG_UTILS_DBUS_H_

#include "QDBusConnection"
#include "QDBusError"
#include "linglong/utils/dbus/log.h"
#include "linglong/utils/error/error.h"
#include "tl/expected.hpp"

namespace linglong::utils::dbus {

[[nodiscard]] inline auto registerDBusObject(QDBusConnection conn,
                                             const QString &path,
                                             QObject *obj) -> tl::expected<void, error::Error>
{
    if (conn.registerObject(path, obj)) {
        qCDebug(linglong_utils_dbus) << "register object to dbus on" << path;
        return LINGLONG_OK;
    }

    // FIXME: use real ERROR CODE defined in API.
    return LINGLONG_ERR(-1, "register dbus object: object existed.");
}

inline void unregisterDBusObject(QDBusConnection conn, const QString &path)
{
    conn.unregisterObject(path);
    qCDebug(linglong_utils_dbus) << "unregister object to dbus on" << path;
}

[[nodiscard]] inline auto registerDBusService(QDBusConnection conn, const QString &serviceName)
  -> error::Result<void>
{
    if (conn.registerService(serviceName)) {
        qCDebug(linglong_utils_dbus) << "register dbus name" << serviceName;
        return LINGLONG_OK;
    }

    error::Error err;
    // FIXME: use real ERROR CODE defined in API.
    if (conn.lastError().isValid()) {
        err =
          LINGLONG_ERR(-1,
                       QString("%1: %2").arg(conn.lastError().name(), conn.lastError().message()))
            .value();
    } else {
        err = LINGLONG_ERR(-1, "service name existed.").value();
    }

    return LINGLONG_EWRAP("register dbus service:", err);
}

[[nodiscard]] inline auto unregisterDBusService(QDBusConnection conn, const QString &serviceName)
  -> tl::expected<void, error::Error>
{
    if (conn.unregisterService(serviceName)) {
        qCDebug(linglong_utils_dbus) << "unregister dbus name" << serviceName;
        return LINGLONG_OK;
    }

    // FIXME: use real ERROR CODE defined in API.
    error::Error err;
    if (conn.lastError().isValid()) {
        err =
          LINGLONG_ERR(-1,
                       QString("%1: %2").arg(conn.lastError().name(), conn.lastError().message()))
            .value();
    } else {
        err = LINGLONG_ERR(-1, "unknown").value();
    }

    return LINGLONG_EWRAP("unregister dbus service:", err);
}

} // namespace linglong::utils::dbus

#endif
