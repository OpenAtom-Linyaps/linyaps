/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/adaptors/app_manager/app_manager1.h"
#include "linglong/dbus_ipc/workaround.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>

auto main(int argc, char *argv[]) -> int
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    auto conn = QDBusConnection::sessionBus();

    linglong::adaptors::app_manger::AppManager1 pma(APP_MANAGER);

    auto registerDBusObjectResult =
            linglong::utils::dbus::registerDBusObject<linglong::service::AppManager>(
                    conn,
                    "/org/deepin/linglong/AppManager",
                    APP_MANAGER);
    auto _ = linglong::utils::finally::finally([&conn] {
        linglong::utils::dbus::unregisterDBusObject(conn, "/org/deepin/linglong/AppManager");
    });
    if (!registerDBusObjectResult) {
        qCritical().noquote() << registerDBusObjectResult.error()->message();
        return -1;
    }

    auto registerDBusServiceResult =
            linglong::utils::dbus::registerDBusService(conn, "org.deepin.linglong.AppManager");
    auto __ = linglong::utils::finally::finally([&conn] {
        auto unregisterDBusServiceResult =
                linglong::utils::dbus::unregisterDBusService(conn,
                                                             "org.deepin.linglong.AppManager");
        if (!unregisterDBusServiceResult) {
            qWarning().noquote() << unregisterDBusServiceResult.error()->message();
        }
    });
    if (!registerDBusServiceResult) {
        qCritical().noquote() << registerDBusServiceResult.error()->message();
        return -1;
    }

    return QCoreApplication::exec();
}
