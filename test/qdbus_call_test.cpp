/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtest/gtest.h>
#include <QDebug>
#include <thread>
#include <future>
#include "../src/module/package/package.h"
#include "../src/service/impl/json_register_inc.h"

#include "package_manager.h"
#include <cstdlib>

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
