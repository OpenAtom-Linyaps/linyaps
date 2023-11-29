/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "ClientApi.h"
#include "linglong/adaptors/app_manager/app_manager1.h"
#include "linglong/dbus_ipc/workaround.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/dbus/register.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>

auto main(int argc, char *argv[]) -> int
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;
    using linglong::utils::dbus::registerDBusObject;
    using linglong::utils::dbus::registerDBusService;
    using linglong::utils::dbus::unregisterDBusObject;
    using linglong::utils::dbus::unregisterDBusService;
    using linglong::utils::finally::finally;

    applicationInitializte();

    registerDBusParam();

    auto conn = QDBusConnection::sessionBus();

    linglong::api::client::ClientApi api;
    // api.setNewServerForAllOperations()

    linglong::repo::OSTreeRepo ostree(linglong::util::getLinglongRootPath(), "", "", api);

    linglong::service::AppManager appManager(ostree);
    linglong::adaptors::app_manger::AppManager1 appManagerAdaport(&appManager);

    auto result = registerDBusObject(conn,
                                     // FIXME: use cmake option
                                     "/org/deepin/linglong/AppManager",
                                     &appManager);
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [&conn] {
        unregisterDBusObject(conn,
                             // FIXME: use cmake option
                             "/org/deepin/linglong/AppManager");
    });
    if (!result.has_value()) {
        qCritical().noquote() << "Launching failed:" << Qt::endl << result.error().message();
        return -1;
    }

    result = registerDBusService(conn,
                                 // FIXME: use cmake option
                                 "org.deepin.linglong.AppManager");
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [&conn] {
        auto result = unregisterDBusService(conn,
                                            // FIXME: use cmake option
                                            "org.deepin.linglong.AppManager");
        if (!result.has_value()) {
            qWarning().noquote() << "During exiting:" << Qt::endl << result.error().message();
        }
    });
    if (!result.has_value()) {
        qCritical().noquote() << "Launching failed:" << Qt::endl << result.error().message();
        return -1;
    }

    return QCoreApplication::exec();
}
