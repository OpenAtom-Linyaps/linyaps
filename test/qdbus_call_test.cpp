/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>
#include <QDebug>
#include <thread>
#include <future>
#include <cstdlib>

#include "src/module/package/package.h"
#include "src/service/impl/json_register_inc.h"
#include "package_manager.h"


/*!
 * start service
 */
void bin_ll_service()
{
    setenv("DISPLAY", ":0", 1);
    setenv("XAUTHORITY", "~/.Xauthority", 1);
    system("../bin/ll-service &");
}

/*!
 * stop service
 */
void kill_ll_service()
{
    system("pkill -f ../bin/ll-service");
}

TEST(QDbusCall, start_qdbus)
{
    // register qdbus type
    RegisterDbusType();

    // start service
    std::thread start_qdbus(bin_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());

    // call dbus
    auto ret_msg = pm.QDbusMessageRet().value();

    for (auto it : ret_msg) {
        qInfo() << "state:\t" << it->state;
        qInfo() << "code:\t" << it->code;
    }
    EXPECT_EQ(1, 1);

    // stop service
    kill_ll_service();
}
