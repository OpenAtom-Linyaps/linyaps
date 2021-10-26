/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

/*
 * 查询测试服务器连接状态
 *
 * @param sIp: 测试服务器ip
 *
 * @return bool: true: 可以连上测试服务器 false:连不上服务器
 */
bool getConnectStatus(QString sIp)
{
    if (sIp.isEmpty()) {
        return false;
    }
    QProcess proc;
    QStringList argstrList;
    argstrList << "-s 1" << "-c 1" << sIp;
    proc.start("ping", argstrList);
    if (!proc.waitForStarted()) {
        qInfo() << "start ping failed!";
        return false;
    }
    proc.waitForFinished(3000);
    QString ret = proc.readAllStandardOutput();
    qInfo() << "ret msg:" << ret;
    bool connect = (ret.indexOf("ttl", Qt::CaseInsensitive) >= 0);
    return connect;
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
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test pkg not in repo
    QString appID = "test.deepin.test";
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
    // 判断是否能访问临时服务器 to do fix
    bool connect = getConnectStatus("10.20.54.2");
    if (!connect) {
        qInfo() << "warning can't connect to test server";
    }
    RetMessageList retMsg = reply.value();
    if (retMsg.size() > 0) {
        auto it = retMsg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, connect);
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
    QString appID = "test.deepin.test";
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

    // 查询是否已安装
    QDBusPendingReply<PKGInfoList> replyQuery = pm.Query({"installed"});
    replyQuery.waitForFinished();
    bool expectRet = true;
    PKGInfoList queryMsg = replyQuery.value();
    for (auto const &it : queryMsg) {
        if (it->appid == "org.deepin.calculator") {
            expectRet = false;
            break;
        }
    }
    QDBusPendingReply<RetMessageList> reply = pm.Install({appID});
    reply.waitForFinished();
    // 判断是否能访问临时服务器 to do fix
    bool connect = getConnectStatus("10.20.54.2");
    if (!connect) {
        expectRet = false;
    }
    RetMessageList retMsg = reply.value();
    if (retMsg.size() > 0) {
        auto it = retMsg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, expectRet);
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
    QFileInfo uapFile(appID);
    QString ouapPath = uapFile.absoluteFilePath();
    bool expectRet = true;
    // 判断文件是否存在
    if (!linglong::util::fileExists(ouapPath)) {
        expectRet = false;
        qInfo() << ouapPath << " not exist";
    }
    // 查询是否已安装
    QDBusPendingReply<PKGInfoList> replyQuery = pm.Query({"installed"});
    replyQuery.waitForFinished();
    PKGInfoList queryMsg = replyQuery.value();
    for (auto const &it : queryMsg) {
        if (it->appid == "org.deepin.calculator") {
            expectRet = false;
            break;
        }
    }
    bool connect = getConnectStatus("10.20.54.2");
    if (!connect) {
        expectRet = false;
    }
    QDBusPendingReply<RetMessageList> reply = pm.Install({ouapPath});
    reply.waitForFinished();
    RetMessageList retMsg = reply.value();
    if (retMsg.size() > 0) {
        auto it = retMsg.at(0);
        qInfo() << "message:\t" << it->message;
        if (!it->state) {
            qInfo() << "code:\t" << it->code;
        }
        EXPECT_EQ(it->state, expectRet);
    }
    // stop service
    stop_ll_service();
}

TEST(Package, query01)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test app not in repo
    auto appID = "test.deepin.test";
    QDBusPendingReply<PKGInfoList> reply = pm.Query({appID});
    reply.waitForFinished();
    PKGInfoList ret_msg = reply.value();
    bool ret = ret_msg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, query02)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test app not in repo
    auto appID = "";
    QDBusPendingReply<PKGInfoList> reply = pm.Query({appID});
    reply.waitForFinished();
    PKGInfoList ret_msg = reply.value();
    bool ret = ret_msg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, list01)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    // test app not in repo
    auto optPara = "";
    QDBusPendingReply<PKGInfoList> reply = pm.Query({optPara});
    reply.waitForFinished();
    PKGInfoList retMsg = reply.value();
    bool ret = retMsg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, list02)
{
    // start service
    std::thread start_qdbus(start_ll_service);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    auto optPara = "installed";
    QDBusPendingReply<PKGInfoList> reply = pm.Query({optPara});
    reply.waitForFinished();
    PKGInfoList retMsg = reply.value();
    bool ret = retMsg.size() > 0 ? true : false;

    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    bool expectRet = true;
    if (!linglong::util::fileExists(dbPath)) {
        expectRet = false;
        qInfo() << "no installed app in system";
    }
    EXPECT_EQ(ret, expectRet);
    // stop service
    stop_ll_service();
}