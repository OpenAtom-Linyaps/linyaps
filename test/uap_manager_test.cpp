/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     liujianqiang <liujianqiang@uniontech.com>
 *
 * Maintainer: liujianqiang <liujianqiang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "uap_manager.h"

#include <gtest/gtest.h>
#include <thread>
#include <future>

#include <QDebug>

#include "module/package/package.h"
#include "service/impl/json_register_inc.h"
#include "service/impl/dbus_retcode.h"
#include "module/util/fs.h"

#define PKGUAPNAME "org.deepin.calculator-1.2.2-x86_64.uap"
#define CONFIGFILE "../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64/uap.json"
#define DATADIR "../../test/data/demo/pkg-demo/org.deepin.calculator/1.2.2/x86_64"
#define UAPTESTDIR "/tmp/uap-test"
#define PKGOUAPNAME "org.deepin.calculator-1.2.2-x86_64.ouap"
#define PKGDATAPATH "/deepin/linglong/layers/org.deepin.calculator/1.2.2"

using namespace linglong::util;
using namespace linglong::dbus;

/*!
 * 开启服务
 */
void startUapLinglongService()
{
    setenv("DISPLAY", ":0", 1);
    setenv("XAUTHORITY", "~/.Xauthority", 1);
    // 需要进入到test目录进行ll-test
    system("../bin/ll-service &");
}

/*!
 * 停止服务
 */
void stopUapLinglongService()
{
    system("pkill -f ../bin/ll-service");
}

TEST(UapManager, build)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());
    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }
    // 停止服务
    stopUapLinglongService();
}

TEST(UapManager, buildouap)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);
    //ouap包名
    QString pkgOuapName = QString(PKGOUAPNAME);
    //uap路径
    QString uapPath = uapTestDir + QString("/uap/") + pkgUapName;
    //ouap路径
    QString ouapPath = uapTestDir + QString("/ouap/") + pkgOuapName;

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());

    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //调用在线包制作dbus接口
    QDBusPendingReply<bool> replyOuap = pm.BuildOuap(uapPath, uapTestDir + QString("/repo"), uapTestDir + QString("/ouap"));
    //等待执行完成
    replyOuap.waitForFinished();
    //接受返回值
    bool retBuildOuap = replyOuap.value();
    //判断返回值是否为true
    EXPECT_EQ(retBuildOuap, true);
    //判断ouap是否生成
    EXPECT_EQ(fileExists(ouapPath), true);
    //判断仓库是否生产
    EXPECT_EQ(fileExists(uapTestDir + QString("/repo/config")), true);

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }

    // 停止服务
    stopUapLinglongService();
}

TEST(UapManager, extract)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);
    //ouap包名
    QString pkgOuapName = QString(PKGOUAPNAME);
    //uap路径
    QString uapPath = uapTestDir + QString("/uap/") + pkgUapName;
    //ouap路径
    QString ouapPath = uapTestDir + QString("/ouap/") + pkgOuapName;

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());

    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //调用在线包制作dbus接口
    QDBusPendingReply<bool> replyOuap = pm.BuildOuap(uapPath, uapTestDir + QString("/repo"), uapTestDir + QString("/ouap"));
    //等待执行完成
    replyOuap.waitForFinished();
    //接受返回值
    bool retBuildOuap = replyOuap.value();
    //判断返回值是否为true
    EXPECT_EQ(retBuildOuap, true);
    //判断ouap是否生成
    EXPECT_EQ(fileExists(ouapPath), true);
    //判断仓库是否生产
    EXPECT_EQ(fileExists(uapTestDir + QString("/repo/config")), true);

    //解压离线包uap
    //调用离线包解压dbus接口
    QDBusPendingReply<bool> replyUapExtract = pm.Extract(uapPath, uapTestDir + QString("/uap-extract"));
    //等待执行完成
    replyUapExtract.waitForFinished();
    //接受返回值
    bool retExtractUap = replyUapExtract.value();
    //判断返回值是否为true
    EXPECT_EQ(retExtractUap, true);
    //判断uap-1是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/uap-1")), true);
    //判断.uap-1.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.uap-1.sig")), true);
    //判断data.tgz是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/data.tgz")), true);
    //判断.data.tgz.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.data.tgz.sig")), true);

    //解压在线包ouap
    //调用在线包解压dbus接口
    QDBusPendingReply<bool> replyOuapExtract = pm.Extract(ouapPath, uapTestDir + QString("/ouap-extract"));
    //等待执行完成
    replyOuapExtract.waitForFinished();
    //接受返回值
    bool retExtractOuap = replyOuapExtract.value();
    //判断返回值是否为true
    EXPECT_EQ(retExtractOuap, true);
    //判断uap-1是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/uap-1")), true);
    //判断.uap-1.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/.uap-1.sig")), true);
    //判断data.tgz是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/data.tgz")), false);
    //判断.data.tgz.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.data.tgz.sig")), true);

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }

    // 停止服务
    stopUapLinglongService();
}

TEST(UapManager, getinfo)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);
    //ouap包名
    QString pkgOuapName = QString(PKGOUAPNAME);
    //uap路径
    QString uapPath = uapTestDir + QString("/uap/") + pkgUapName;
    //ouap路径
    QString ouapPath = uapTestDir + QString("/ouap/") + pkgOuapName;

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());

    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //调用在线包制作dbus接口
    QDBusPendingReply<bool> replyOuap = pm.BuildOuap(uapPath, uapTestDir + QString("/repo"), uapTestDir + QString("/ouap"));
    //等待执行完成
    replyOuap.waitForFinished();
    //接受返回值
    bool retBuildOuap = replyOuap.value();
    //判断返回值是否为true
    EXPECT_EQ(retBuildOuap, true);
    //判断ouap是否生成
    EXPECT_EQ(fileExists(ouapPath), true);
    //判断仓库是否生产
    EXPECT_EQ(fileExists(uapTestDir + QString("/repo/config")), true);

    //获取uap包info文件
    //调用离线包信息获取dbus接口
    QDBusPendingReply<bool> replyUapGetInfo = pm.GetInfo(uapPath, uapTestDir + QString("/uap-getinfo"));
    //等待执行完成
    replyUapGetInfo.waitForFinished();
    //接受返回值
    bool retUapGetInfo = replyUapGetInfo.value();
    //判断返回值是否为true
    EXPECT_EQ(retUapGetInfo, true);
    //判断info文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-getinfo/") + pkgUapName + QString(".info")), true);

    //获取ouap包info文件
    //调用在线包信息获取dbus接口
    QDBusPendingReply<bool> repluOuapGetInfo = pm.GetInfo(ouapPath, uapTestDir + QString("/ouap-getinfo"));
    //等待执行完成
    repluOuapGetInfo.waitForFinished();
    //接受返回值
    bool retOuapGetInfo = repluOuapGetInfo.value();
    //判断返回值是否为true
    EXPECT_EQ(retOuapGetInfo, true);
    //判断info文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-getinfo/") + pkgOuapName + QString(".info")), true);

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }
    // 停止服务
    stopUapLinglongService();
}

TEST(UapManager, check)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);
    //ouap包名
    QString pkgOuapName = QString(PKGOUAPNAME);
    //uap路径
    QString uapPath = uapTestDir + QString("/uap/") + pkgUapName;
    //ouap路径
    QString ouapPath = uapTestDir + QString("/ouap/") + pkgOuapName;

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());

    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //调用在线包制作dbus接口
    QDBusPendingReply<bool> replyOuap = pm.BuildOuap(uapPath, uapTestDir + QString("/repo"), uapTestDir + QString("/ouap"));
    //等待执行完成
    replyOuap.waitForFinished();
    //接受返回值
    bool retBuildOuap = replyOuap.value();
    //判断返回值是否为true
    EXPECT_EQ(retBuildOuap, true);
    //判断ouap是否生成
    EXPECT_EQ(fileExists(ouapPath), true);
    //判断仓库是否生产
    EXPECT_EQ(fileExists(uapTestDir + QString("/repo/config")), true);

    //解压离线包uap
    //调用离线包解压dbus接口
    QDBusPendingReply<bool> replyUapExtract = pm.Extract(uapPath, uapTestDir + QString("/uap-extract"));
    //等待执行完成
    replyUapExtract.waitForFinished();
    //接受返回值
    bool retExtractUap = replyUapExtract.value();
    //判断返回值是否为true
    EXPECT_EQ(retExtractUap, true);
    //判断uap-1是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/uap-1")), true);
    //判断.uap-1.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.uap-1.sig")), true);
    //判断data.tgz是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/data.tgz")), true);
    //判断.data.tgz.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.data.tgz.sig")), true);

    //解压在线包ouap
    //调用在线包解压dbus接口
    QDBusPendingReply<bool> replyOuapExtract = pm.Extract(ouapPath, uapTestDir + QString("/ouap-extract"));
    //等待执行完成
    replyOuapExtract.waitForFinished();
    //接受返回值
    bool retExtractOuap = replyOuapExtract.value();
    //判断返回值是否为true
    EXPECT_EQ(retExtractOuap, true);
    //判断uap-1是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/uap-1")), true);
    //判断.uap-1.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/.uap-1.sig")), true);
    //判断data.tgz是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/ouap-extract") + QString("/data.tgz")), false);
    //判断.data.tgz.sig是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap-extract") + QString("/.data.tgz.sig")), true);

    //check uap包
    //调用离线包check dbus接口
    QDBusPendingReply<bool> replyUapCheck = pm.Check(uapTestDir + QString("/uap-extract"));
    //等待执行完成
    replyUapCheck.waitForFinished();
    //接受返回值
    bool retUapCheck = replyUapCheck.value();
    //判断返回值是否为true
    EXPECT_EQ(retUapCheck, true);

    //check ouap包
    //调用在线包check dbus接口
    QDBusPendingReply<bool> replyOuapCheck = pm.Check(uapTestDir + QString("/ouap-extract"));
    //等待执行完成
    replyOuapCheck.waitForFinished();
    //接受返回值
    bool retOuapCheck = replyOuapCheck.value();
    //判断返回值是否为true
    EXPECT_EQ(retOuapCheck, true);

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }
    // 停止服务
    stopUapLinglongService();
}

TEST(UapManager, install)
{
    //uap包名
    QString pkgUapName = QString(PKGUAPNAME);
    //uap配置文件
    QString configFile = QString(CONFIGFILE);
    //uap数据目录
    QString dataDir = QString(DATADIR);
    //uap测试目录
    QString uapTestDir = QString(UAPTESTDIR);
    //ouap包名
    QString pkgOuapName = QString(PKGOUAPNAME);
    //uap路径
    QString uapPath = uapTestDir + QString("/uap/") + pkgUapName;
    //ouap路径
    QString ouapPath = uapTestDir + QString("/ouap/") + pkgOuapName;

    //创建空的测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
        createDir(uapTestDir);
    }

    //注册返回值类型
    RegisterDbusType();

    // 开启服务
    std::thread start_qdbus(startUapLinglongService);
    start_qdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //创建接口对象
    ComDeepinLinglongUapManagerInterface pm("com.deepin.linglong.AppManager",
                                            "/com/deepin/linglong/UapManager",
                                            QDBusConnection::sessionBus());

    //设置timeout
    pm.setTimeout(1000 * 60 * 10);

    //调用离线包制作dbus接口
    QDBusPendingReply<bool> reply = pm.BuildUap(configFile, dataDir, uapTestDir + QString("/uap"));
    //等待执行完成
    reply.waitForFinished();
    //接受返回值
    bool retBuildUap = reply.value();
    //判断uap文件是否生成
    EXPECT_EQ(fileExists(uapTestDir + QString("/uap/") + pkgUapName), true);
    //判断返回值是否为true
    EXPECT_EQ(retBuildUap, true);

    //调用在线包制作dbus接口
    QDBusPendingReply<bool> replyOuap = pm.BuildOuap(uapPath, uapTestDir + QString("/repo"), uapTestDir + QString("/ouap"));
    //等待执行完成
    replyOuap.waitForFinished();
    //接受返回值
    bool retBuildOuap = replyOuap.value();
    //判断返回值是否为true
    EXPECT_EQ(retBuildOuap, true);
    //判断ouap是否生成
    EXPECT_EQ(fileExists(ouapPath), true);
    //判断仓库是否生产
    EXPECT_EQ(fileExists(uapTestDir + QString("/repo/config")), true);

    //检查离线包安装状态
    bool rmPkgData = true;
    QString uapDataPath = QString(PKGDATAPATH);
    if (dirExists(uapDataPath)) {
        rmPkgData = false;
    }

    //安装离线包
    QDBusPendingReply<RetMessageList> replyInstall = pm.Install({uapPath});
    //等待执行完成
    replyInstall.waitForFinished();
    //获取返回值
    RetMessageList retInstall = replyInstall.value();
    //获取返回值数据
    if (retInstall.size() > 0) {
        auto retMessage = retInstall.at(0);
        qInfo() << "message:\t" << retMessage->message;
        //判断获取的软件包名
        EXPECT_EQ(retMessage->message, pkgUapName);
        if (!retMessage->state) {
            qInfo() << "code:\t" << retMessage->code;
            //判断安装状态
            EXPECT_EQ(retMessage->code, RetCode(RetCode::uap_install_success));
        }
        //判断安装结果
        EXPECT_EQ(retMessage->state, true);
    }

    //删除uap包安装数据
    if (rmPkgData) {
        removeDir(uapDataPath);
    }

    //删除测试目录
    if (dirExists(uapTestDir)) {
        removeDir(uapTestDir);
    }
    // 停止服务
    stopUapLinglongService();
}
