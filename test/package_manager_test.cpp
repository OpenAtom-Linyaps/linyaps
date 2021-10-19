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

/*!
 * start service
 */
void start_ll_service()
{
    setenv("DISPLAY", ":0", 1);
    setenv("XAUTHORITY", "~/.Xauthority", 1);
    // 需要进入到test目录进行ll-test
    system("../bin/ll-service &");
}

/*!
 * stop service
 */
void stop_ll_service()
{
    system("pkill -f ../bin/ll-service");
}

TEST(Package, downloadtest01)
{
    RegisterDbusType();

    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test err para
    QString appID = "";
    QString curPath = QDir::currentPath();
    QDBusPendingReply<RetMessageList> reply = pm.Download({appID}, curPath);
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, false);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, downloadtest02)
{
    RegisterDbusType();

    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test pkg not in repo
    QString appID = "org.deepin.qqtest";
    QString curPath = QDir::currentPath();
    QDBusPendingReply<RetMessageList> reply = pm.Download({appID}, curPath);
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, false);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, downloadtest03)
{
    RegisterDbusType();

    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // call dbus
    QString appID = "org.deepin.calculator";
    QString curPath = QDir::currentPath();
    QDBusPendingReply<RetMessageList> reply = pm.Download({appID}, curPath);
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, true);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, install01)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // call dbus
    QString appID = "";
    QDBusPendingReply<RetMessageList> reply = pm.Install({appID});
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, false);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, install02)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test pkg not in repo
    QString appID = "org.deepin.qqtest";
    QDBusPendingReply<RetMessageList> reply = pm.Install({appID});
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, false);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, install03)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // call dbus
    QString appID = "org.deepin.calculator";
    QDBusPendingReply<RetMessageList> reply = pm.Install({appID});
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, true);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, install04)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    //if the uap file in the cmd dir success
    QString appID = "org.deepin.calculator-1.2.2-x86_64.uap";
    QFileInfo uap_fs(appID);
    QDBusPendingReply<RetMessageList> reply = pm.Install({uap_fs.absoluteFilePath()});
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, true);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, install05)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    QString appID = "org.deepin.calculator-1.2.2-x86_64.ouap";
    QFileInfo uap_fs(appID);
    QDBusPendingReply<RetMessageList> reply = pm.Install({uap_fs.absoluteFilePath()});
    reply.waitForFinished();
    RetMessageList ret_msg = reply.value();
    if (ret_msg.size() > 0) {
        auto it = ret_msg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, true);
    }
    // stop service
    stop_ll_service();
}
