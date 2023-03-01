/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "app_manager.h"
#include "package_manager.h"
#include "src/module/dbus_ipc/param_option.h"
#include "src/module/dbus_ipc/register_meta_type.h"
#include "src/module/dbus_ipc/reply.h"
#include "src/module/flatpak/flatpak_manager.h"
#include "src/module/package/package.h"
#include "src/module/util/app_status.h"

#include <QDebug>

#include <future>
#include <thread>

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
 * @return bool: true: 可以连上测试服务器 false:连不上服务器
 */
bool getConnectStatus()
{
    // curl -o /dev/null -s -m 10 --connect-timeout 10 -w %{http_code}
    // "https://linglong-api-dev.deepin.com/ostree/" 返回200表示通
    const QString testServer = "https://mirror-repo-linglong.deepin.com/repo/";
    QProcess proc;
    QStringList argstrList;
    argstrList << "-o"
               << "/dev/null"
               << "-s"
               << "-m"
               << "10"
               << "--connect-timeout"
               << "10"
               << "-w"
               << "%{http_code}" << testServer;
    argstrList.join(" ");
    proc.start("curl", argstrList);
    if (!proc.waitForStarted()) {
        qCritical() << "start curl failed!";
        return false;
    }
    proc.waitForFinished(3000);
    QString ret = proc.readAllStandardOutput();
    qInfo() << "ret msg:" << ret;
    bool connect = (ret.indexOf("200", Qt::CaseInsensitive) >= 0);
    return connect;
}

TEST(Package, install01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    linglong::service::registerAllMetaType();
    linglong::package::registerAllMetaType();

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::InstallParamOption installParam;
    installParam.appId = "com.deepin.linglong.test";
    QDBusPendingReply<linglong::service::Reply> reply = pm.Install(installParam);
    reply.waitForFinished();
    linglong::service::Reply retReply = reply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstallSuccess));
    // stop service
    stop_ll_service();
}

TEST(Package, install02)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "org.deepin.calculator";
    uninstallParam.version = "5.7.16";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::InstallParamOption installParam;
    installParam.appId = "org.deepin.calculator";
    installParam.version = "5.7.16";
    dbusReply = pm.Install(installParam);
    dbusReply.waitForFinished();

    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstallSuccess));
    // stop service
    stop_ll_service();
}

TEST(Package, update01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "org.deepin.calculator";
    uninstallParam.version = "5.7.16.1";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::ParamOption parmOption;
    parmOption.appId = "org.deepin.calculator";
    dbusReply = pm.Update(parmOption);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    bool expectRet = false;
    bool connect = getConnectStatus();
    if (!connect) {
        expectRet = false;
    }
    if (expectRet) {
        EXPECT_EQ(retReply.code, STATUS_CODE(kErrorPkgUpdateSuccess));
    } else {
        EXPECT_NE(retReply.code, STATUS_CODE(kErrorPkgUpdateSuccess));
    }
}

TEST(Package, install03)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    // 重复安装
    linglong::service::InstallParamOption installParam;
    installParam.appId = "org.deepin.calculator";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Install(installParam);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstallSuccess));
    stop_ll_service();
}

TEST(Package, install04)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    linglong::service::InstallParamOption installParam;
    installParam.appId = "";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Install(installParam);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstallSuccess));

    stop_ll_service();
}

TEST(Package, install05)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    linglong::service::InstallParamOption installParam;
    installParam.appId = "org.deepin.calculator";
    installParam.arch = "arm64";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Install(installParam);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstallSuccess));

    // stop service
    stop_ll_service();
}

TEST(Package, install06)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());

    linglong::service::InstallParamOption installParam;
    installParam.appId = "com.belmoussaoui.Decoder";
    installParam.repoPoint = "flatpak";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Install(installParam);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kPkgInstalling));

    // stop service
    stop_ll_service();
}

TEST(Package, run01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongAppManagerInterface pm("org.deepin.linglong.AppManager",
                                            "/org/deepin/linglong/AppManager",
                                            QDBusConnection::sessionBus());

    linglong::service::RunParamOption paramOption;
    paramOption.appId = "com.belmoussaoui.Decoder";
    paramOption.repoPoint = "flatpak";

    QString runtimeRootPath =
            linglong::flatpak::FlatpakManager::instance()->getRuntimePath(paramOption.appId);
    QString appPath = linglong::flatpak::FlatpakManager::instance()->getAppPath(paramOption.appId);
    QStringList desktopPath =
            linglong::flatpak::FlatpakManager::instance()->getAppDesktopFileList(paramOption.appId);

    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Start(paramOption);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kFail));

    // stop service
    stop_ll_service();
}

TEST(Package, run02)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongAppManagerInterface pm("org.deepin.linglong.AppManager",
                                            "/org/deepin/linglong/AppManager",
                                            QDBusConnection::sessionBus());

    linglong::service::RunParamOption paramOption;
    paramOption.appId = "org.deepin.calculator";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Start(paramOption);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kFail));

    // stop service
    stop_ll_service();
}

TEST(Package, run03)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongAppManagerInterface pm("org.deepin.linglong.AppManager",
                                            "/org/deepin/linglong/AppManager",
                                            QDBusConnection::sessionBus());
    linglong::service::ExecParamOption paramOption;
    paramOption.containerID = "org.deepin.test";
    paramOption.cmd = "/bin/bash";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Exec(paramOption);
    dbusReply.waitForFinished();
    linglong::service::Reply retReply = dbusReply.value();

    EXPECT_NE(retReply.code, STATUS_CODE(kSuccess));

    // stop service
    stop_ll_service();
}

TEST(Package, query01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    // test app not in repo
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = QString();

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, query02)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    // test app not in repo
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "org.test1";

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, query03)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "cal";

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? false : true;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, query04)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "com.360.browser-stable";

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? false : true;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, query05)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    // test flatpak app
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "com.belmoussaoui.Decoder";
    paramOption.repoPoint = "flatpak";

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? false : true;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, list01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "";

    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() == 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, list02)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "installed";
    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() > 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, list03)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::QueryParamOption paramOption;
    paramOption.appId = "installed";
    paramOption.repoPoint = "flatpak";
    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.Query(paramOption);
    dbusReply.waitForFinished();
    linglong::service::QueryReply reply = dbusReply.value();
    linglong::util::getAppMetaInfoListByJson(reply.result, retMsg);
    bool ret = retMsg.size() > 0 ? true : false;
    EXPECT_EQ(ret, true);
    // stop service
    stop_ll_service();
}

TEST(Package, ps01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongAppManagerInterface pm("org.deepin.linglong.AppManager",
                                            "/org/deepin/linglong/AppManager",
                                            QDBusConnection::sessionBus());

    linglong::service::RunParamOption paramOption;
    paramOption.appId = "org.deepin.calculator";
    linglong::package::AppMetaInfoList retMsg;
    QDBusPendingReply<linglong::service::Reply> reply = pm.Start(paramOption);
    reply.waitForFinished();

    QDBusPendingReply<linglong::service::QueryReply> dbusReply = pm.ListContainer();
    dbusReply.waitForFinished();

    EXPECT_NE(dbusReply.value().code, STATUS_CODE(kFail));

    // stop service
    stop_ll_service();
}

TEST(Package, kill)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongAppManagerInterface pm("org.deepin.linglong.AppManager",
                                            "/org/deepin/linglong/AppManager",
                                            QDBusConnection::sessionBus());

    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Stop("org.deepin.calculator");
    dbusReply.waitForFinished();

    EXPECT_NE(dbusReply.value().code, STATUS_CODE(kSuccess));
    // stop service
    stop_ll_service();
}

TEST(Package, uninstall01)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "org.deepin.calculator";
    uninstallParam.version = "5.7.16.1";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgUninstallSuccess));

    // stop service
    stop_ll_service();
}

TEST(Package, uninstall02)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "org.deepin.calculator123";
    uninstallParam.version = "5.7.16.1";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgUninstallSuccess));

    // stop service
    stop_ll_service();
}

TEST(Package, uninstall03)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "";
    uninstallParam.version = "5.7.16.1";
    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgUninstallSuccess));

    // stop service
    stop_ll_service();
}

TEST(Package, uninstall04)
{
    // start service
    std::thread startQdbus(start_ll_service);
    startQdbus.detach();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    OrgDeepinLinglongPackageManagerInterface pm("org.deepin.linglong.PackageManager",
                                                "/org/deepin/linglong/PackageManager",
                                                QDBusConnection::systemBus());
    linglong::service::UninstallParamOption uninstallParam;
    uninstallParam.appId = "com.belmoussaoui.Decoder";
    uninstallParam.repoPoint = "flatpak";

    QDBusPendingReply<linglong::service::Reply> dbusReply = pm.Uninstall(uninstallParam);
    dbusReply.waitForFinished();

    linglong::service::Reply retReply = dbusReply.value();
    EXPECT_NE(retReply.code, STATUS_CODE(kPkgUninstallSuccess));

    // stop service
    stop_ll_service();
}
