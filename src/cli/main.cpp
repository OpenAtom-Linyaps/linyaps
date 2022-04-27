/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QMap>

#include "cmd/command_helper.h"
#include "module/package/package.h"
#include "module/util/package_manager_param.h"
#include "service/impl/json_register_inc.h"
#include "service/impl/package_manager.h"
#include "package_manager.h"
#include "module/runtime/runtime.h"
#include "module/util/app_status.h"
#include "module/util/xdg.h"
#include "module/util/env.h"
#include "module/util/log_handler.h"
#include "module/util/sysinfo.h"

/**
 * @brief 注册QT对象类型
 *
 */
static void qJsonRegisterAll()
{
    linglong::package::registerAllMetaType();
    linglong::runtime::registerAllMetaType();
    linglong::service::registerAllMetaType();
}

/**
 * @brief 输出flatpak命令的查询结果
 *
 * @param appMetaInfoList 软件包元信息列表
 *
 */
void printFlatpakAppInfo(linglong::package::AppMetaInfoList appMetaInfoList)
{
    if (appMetaInfoList.size() > 0) {
        if ("flatpaklist" == appMetaInfoList.at(0)->appId) {
            qInfo("%-48s%-16s%-16s%-12s%-12s%-12s%-12s", "Description", "Application", "Version", "Branch", "Arch",
                  "Origin", "Installation");
        } else {
            qInfo("%-72s%-16s%-16s%-12s%-12s", "Description", "Application", "Version", "Branch", "Remotes");
        }
        QString ret = appMetaInfoList.at(0)->description;
        QStringList strList = ret.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
        for (int i = 0; i < strList.size(); ++i) {
            qInfo().noquote() << strList[i].simplified();
        }
    }
}

/**
 * @brief 统计字符串中中文字符的个数
 *
 * @param name 软件包名称
 * @return int 中文字符个数
 */
int getUnicodeNum(const QString &name)
{
    int num = 0;
    int count = name.count();
    for (int i = 0; i < count; i++) {
        QChar ch = name.at(i);
        ushort decode = ch.unicode();
        if (decode >= 0x4E00 && decode <= 0x9FA5) {
            num++;
        }
    }
    return num;
}

/**
 * @brief 输出软件包的查询结果
 *
 * @param appMetaInfoList 软件包元信息列表
 *
 */
void printAppInfo(linglong::package::AppMetaInfoList appMetaInfoList)
{
    if (appMetaInfoList.size() > 0) {
        qInfo("\033[1m\033[38;5;214m%-32s%-32s%-16s%-12s%-s\033[0m", qUtf8Printable("appId"), qUtf8Printable("name"),
              qUtf8Printable("version"), qUtf8Printable("arch"), qUtf8Printable("description"));
        for (auto const &it : appMetaInfoList) {
            QString simpleDescription = it->description.trimmed();
            if (simpleDescription.length() > 56) {
                simpleDescription = it->description.trimmed().left(53) + "...";
            }
            QString appId = it->appId.trimmed();
            QString name = it->name.trimmed();
            if (name.length() > 32) {
                name = it->name.trimmed().left(29) + "...";
            }
            if (appId.length() > 32) {
                name.push_front(" ");
            }
            int count = getUnicodeNum(name);
            int length = simpleDescription.length() < 56 ? simpleDescription.length() : 56;
            qInfo().noquote() << QString("%1%2%3%4%5")
                                     .arg(appId, -32, QLatin1Char(' '))
                                     .arg(name, count - 32, QLatin1Char(' '))
                                     .arg(it->version.trimmed(), -16, QLatin1Char(' '))
                                     .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
                                     .arg(simpleDescription, -length, QLatin1Char(' '));
        }
    } else {
        qInfo().noquote() << "app not found in repo";
    }
}

/**
 * @brief 检测ll-service dbus服务是否已经启动，未启动则启动
 *
 * @param packageManager ll-service dbus服务
 *
 */
void checkAndStartService(ComDeepinLinglongPackageManagerInterface &packageManager)
{
    const auto kStatusActive = "active";
    QDBusReply<QString> status = packageManager.Status();
    // FIXME: should use more precision to check status
    if (kStatusActive != status.value()) {
        QProcess process;
        process.setProgram("ll-service");
        process.setStandardOutputFile("/dev/null");
        process.setStandardErrorFile("/dev/null");
        process.setArguments({});
        process.startDetached();
    }

    // todo: check if service is running
    for (int i = 0; i < 10; ++i) {
        status = packageManager.Status();
        if (kStatusActive == status.value()) {
            return;
        }
        QThread::sleep(1);
    }

    qCritical() << "start ll-service failed";
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    // 安装消息处理函数
    linglong::util::LogHandler::instance()->installMessageHandler();

    // 注册QT对象类型
    qJsonRegisterAll();

    QCommandLineParser parser;
    parser.addHelpOption();
    QStringList subCommandList = {"run",     "ps",        "exec",   "kill",  "download",
                                  "install", "uninstall", "update", "query", "list"};

    parser.addPositionalArgument("subcommand", subCommandList.join("\n"), "subcommand [sub-option]");

    // TODO: for debug now
    auto optDefaultConfig = QCommandLineOption("default-config", "default config json filepath", "");
    parser.addOption(optDefaultConfig);

    parser.parse(QCoreApplication::arguments());

    auto configPath = parser.value(optDefaultConfig);
    if (configPath.isEmpty()) {
        configPath = ":/config.json";
    }

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    ComDeepinLinglongPackageManagerInterface packageManager(
        "com.deepin.linglong.AppManager", "/com/deepin/linglong/PackageManager", QDBusConnection::sessionBus());
    checkAndStartService(packageManager);
    PackageManager *noDbusPackageManager = PackageManager::instance();
    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        {"run", // 启动玲珑应用
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("run", "run application", "run");
             parser.addPositionalArgument("appId", "application id", "com.deepin.demo");

             auto optExec = QCommandLineOption("exec", "run exec", "/bin/bash");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.addOption(optExec);

             auto optNoProxy = QCommandLineOption("no-proxy", "whether to use dbus proxy in box", "");

             auto optNameFilter = QCommandLineOption("filter-name", "dbus name filter to use",
                                                     "--filter-name=com.deepin.linglong.AppManager", "");
             auto optPathFilter = QCommandLineOption("filter-path", "dbus path filter to use",
                                                     "--filter-path=/com/deepin/linglong/PackageManager", "");
             auto optInterfaceFilter = QCommandLineOption("filter-interface", "dbus interface filter to use",
                                                          "--filter-interface=com.deepin.linglong.PackageManager", "");
             parser.addOption(optNoProxy);
             parser.addOption(optNameFilter);
             parser.addOption(optPathFilter);
             parser.addOption(optInterfaceFilter);
             parser.process(app);
             auto repoType = parser.value(optRepoPoint);
             if ((!repoType.isEmpty() && "flatpak" != repoType)) {
                 parser.showHelp();
                 return -1;
             }
             auto args = parser.positionalArguments();
             auto appId = args.value(1);
             if (appId.isEmpty()) {
                 parser.showHelp();
                 return -1;
             }

             auto exec = parser.value(optExec);
             // 转化特殊字符
             args = linglong::util::convertSpecialCharacters(args);

             // 移除run appid两个参数 获取 exec 执行参数
             // eg: ll-cli run deepin-music --exec deepin-music /usr/share/music/test.mp3
             // exec = "deepin-music /usr/share/music/test.mp3"
             QString desktopArgs;
             desktopArgs.clear();
             if (args.length() > 2 && !exec.isEmpty()) {
                 args.removeAt(0);
                 args.removeAt(0);
                 desktopArgs = args.join(" ");
                 exec = QStringList {exec, desktopArgs}.join(" ");
             }

             linglong::service::RunParamOption paramOption;
             // appId format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appId.split("/");
             paramOption.appId = appId;
             if (appInfoList.size() > 1) {
                 paramOption.appId = appInfoList.at(0);
                 paramOption.version = appInfoList.at(1);
             }
             if (!repoType.isEmpty()) {
                 paramOption.repoPoint = repoType;
             }

             if (!exec.isEmpty()) {
                 paramOption.exec = exec;
             }

             // 获取用户环境变量
             QStringList envList = COMMAND_HELPER->getUserEnv(linglong::util::envList);
             if (!envList.isEmpty()) {
                 paramOption.appEnv = envList.join(",");
             }

             // 判断是否设置了no-proxy参数
             paramOption.noDbusProxy = parser.isSet(optNoProxy);
             if (!parser.isSet(optNoProxy)) {
                 // FIX to do only deal with session bus
                 paramOption.busType = "session";
                 paramOption.filterName = parser.value(optNameFilter);
                 paramOption.filterPath = parser.value(optPathFilter);
                 paramOption.filterInterface = parser.value(optInterfaceFilter);
             }

             QDBusPendingReply<linglong::service::Reply> dbusReply = packageManager.Start(paramOption);
             dbusReply.waitForFinished();
             linglong::service::Reply reply = dbusReply.value();
             if (reply.code != 0) {
                 qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                 return -1;
             }
             return 0;
         }},
        {"exec", // 进入玲珑沙箱
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("containerId", "container id", "aebbe2f455cf443f89d5c92f36d154dd");
             parser.addPositionalArgument("exec", "exec command in container", "/bin/bash");
             parser.process(app);

             auto containerId = parser.positionalArguments().value(1);
             if (containerId.isEmpty()) {
                 parser.showHelp();
                 return -1;
             }

             auto cmd = parser.positionalArguments().value(2);
             if (cmd.isEmpty()) {
                 parser.showHelp();
                 return -1;
             }

             auto pid = containerId.toInt();

             return COMMAND_HELPER->namespaceEnter(pid, QStringList {cmd});
         }},
        {"ps", // 查看玲珑沙箱进程
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ps", "show running applications", "ps");

             auto optOutputFormat = QCommandLineOption("output-format", "json/console", "console");
             parser.addOption(optOutputFormat);

             parser.process(app);

             auto outputFormat = parser.value(optOutputFormat);
             auto containerList = packageManager.ListContainer().value();
             COMMAND_HELPER->showContainer(containerList, outputFormat);
             return 0;
         }},
        {"kill", // 关闭玲珑沙箱
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("kill", "kill container with id", "kill");
             parser.addPositionalArgument("container-id", "container id", "");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             auto containerId = args.value(1).trimmed();
             if (containerId.isEmpty()) {
                 parser.showHelp();
                 return -1;
             }
             // TODO: show kill result
             QDBusPendingReply<linglong::service::Reply> dbusReply = packageManager.Stop(containerId);
             dbusReply.waitForFinished();
             linglong::service::Reply reply = dbusReply.value();
             if (reply.code != STATUS_CODE(ErrorPkgKillSuccess)) {
                 qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                 return -1;
             }

             qInfo().noquote() << reply.message;
             return 0;
         }},
        {"download", // TODO: download命令当前没用到
         [&](QCommandLineParser &parser) -> int {
             // auto optArch = QCommandLineOption("arch", "cpu arch", "cpu arch", "x86_64");
             QString curPath = QDir::currentPath();
             // qDebug() << curPath;
             // ll-cli download org.deepin.calculator -d ./test 无-d 参数默认当前路径
             auto optDownload = QCommandLineOption("d", "dest path to save app", "dest path to save app", curPath);

             parser.clearPositionalArguments();
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             parser.addOption(optDownload);
             parser.process(app);
             auto args = parser.positionalArguments();
             // 第一个参数为命令字
             linglong::service::DownloadParamOption downloadParamOption;
             QStringList appInfoList = args.value(1).split("/");
             if (appInfoList.size() > 2) {
                 downloadParamOption.appId = appInfoList.at(0);
                 downloadParamOption.version = appInfoList.at(1);
                 downloadParamOption.arch = appInfoList.at(2);
             }
             downloadParamOption.savePath = parser.value(optDownload);

             packageManager.setTimeout(1000 * 60 * 60 * 24);
             QDBusPendingReply<linglong::service::Reply> reply = packageManager.Download(downloadParamOption);
             reply.waitForFinished();
             linglong::service::Reply retReply = reply.value();
             qInfo().noquote() << retReply.code << retReply.message;
             return retReply.code;
         }},
        {"install", // 下载玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install an application", "install");
             parser.addPositionalArgument("app", "appId version arch", "com.deepin.demo/1.2.1/x86_64");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             auto optNoDbus = QCommandLineOption("nodbus", "execute cmd directly, not via dbus", "");
             parser.addOption(optNoDbus);
             parser.process(app);
             QStringList appList = parser.positionalArguments();

             // auto appId = args.value(1);
             auto repoType = parser.value(optRepoPoint);
             if (appList.isEmpty() || (!repoType.isEmpty() && "flatpak" != repoType)) {
                 parser.showHelp(-1);
                 return -1;
             }
             // 设置 24 h超时
             packageManager.setTimeout(1000 * 60 * 60 * 24);
             QDBusPendingReply<linglong::service::Reply> reply;

             // appId format: org.deepin.calculator/1.2.6 in multi-version
             foreach (const QString &app, appList) {
                 if ("install" == app) {
                     continue;
                 }
                 linglong::service::InstallParamOption installParamOption;
                 installParamOption.nodbus = parser.isSet(optNoDbus);
                 installParamOption.repoPoint = parser.value(optRepoPoint);

                 QStringList appInfoList = app.split("/");
                 installParamOption.appId = appInfoList.at(0);
                 installParamOption.arch = linglong::util::hostArch();
                 if (appInfoList.size() == 2) {
                     installParamOption.version = appInfoList.at(1);
                 } else if (appInfoList.size() == 3) {
                     installParamOption.arch = appInfoList.at(2);
                 }
                 //  appMap[app] = installParamOption;
                 linglong::service::Reply reply;
                 qInfo().noquote() << "install" << app << ", please wait a few minutes...";
                 if (!installParamOption.nodbus) {
                     QDBusPendingReply<linglong::service::Reply> dbusReply = packageManager.Install(installParamOption);
                     reply = dbusReply.value();
                     if (reply.code != STATUS_CODE(pkg_install_success)) {
                         qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                         return -1;
                     } else {
                         qInfo().noquote() << "message:" << reply.message;
                     }
                 } else {
                     reply = noDbusPackageManager->Install(installParamOption);
                     if (reply.code != STATUS_CODE(pkg_install_success)) {
                         qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                         return -1;
                     } else {
                         qInfo().noquote() << "install " << installParamOption.appId << " success";
                     }
                 }
             }
             return 0;
         }},
        {"update", // 更新玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("update", "update an application", "update");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             parser.process(app);
             linglong::service::ParamOption paramOption;
             QStringList appInfoList = parser.positionalArguments().value(1).split("/");
             if (args.isEmpty()) {
                 parser.showHelp(-1);
                 return -1;
             }
             paramOption.appId = appInfoList.at(0).trimmed();
             if (paramOption.appId.isEmpty()) {
                 parser.showHelp(-1);
                 return -1;
             }
             paramOption.arch = linglong::util::hostArch();
             if (appInfoList.size() == 2)
                 paramOption.version = appInfoList.at(1);
             QDBusPendingReply<linglong::service::Reply> dbusReply = packageManager.Update(paramOption);
             linglong::service::Reply reply = dbusReply.value();
             if (reply.code != STATUS_CODE(ErrorPkgUpdateSuccess)) {
                 qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                 return -1;
             }
             qInfo().noquote() << "message:" << reply.message;
             return 0;
         }},
        {"query", // 查询玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("query", "query app info", "query");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             auto optNoCache = QCommandLineOption("force", "query from server directly, not from cache", "");
             parser.addOption(optNoCache);
             parser.process(app);
             auto repoType = parser.value(optRepoPoint);
             if (!repoType.isEmpty() && "flatpak" != repoType) {
                 parser.showHelp(-1);
                 return -1;
             }

             linglong::service::QueryParamOption paramOption;
             paramOption.appId = args.value(1).trimmed();
             if (paramOption.appId.isEmpty()) {
                 parser.showHelp(-1);
                 return -1;
             }
             paramOption.force = parser.isSet(optNoCache);
             paramOption.repoPoint = repoType;
             paramOption.appId = args.value(1);
             linglong::package::AppMetaInfoList appMetaInfoList;
             QDBusPendingReply<linglong::service::QueryReply> dbusReply = packageManager.Query(paramOption);
             dbusReply.waitForFinished();
             linglong::service::QueryReply reply = dbusReply.value();
             linglong::util::getAppMetaInfoListByJson(reply.result, appMetaInfoList);
             if (1 == appMetaInfoList.size() && "flatpakquery" == appMetaInfoList.at(0)->appId) {
                 printFlatpakAppInfo(appMetaInfoList);
             } else {
                 printAppInfo(appMetaInfoList);
             }
             return 0;
         }},
        {"uninstall", // 卸载玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("uninstall", "uninstall an application", "uninstall");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             auto optNoDbus = QCommandLineOption("nodbus", "execute cmd directly, not via dbus", "");
             parser.addOption(optNoDbus);
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto args = parser.positionalArguments();
             auto appInfo = args.value(1);
             auto repoType = parser.value(optRepoPoint);
             if (appInfo.isEmpty() || (!repoType.isEmpty() && repoType != "flatpak")) {
                 parser.showHelp(-1);
                 return -1;
             }
             // TOTO:设置 10 分钟超时 to do
             packageManager.setTimeout(1000 * 60 * 10);
             QDBusPendingReply<linglong::service::Reply> dbusReply;
             linglong::service::UninstallParamOption paramOption;
             // appId format: org.deepin.calculator/1.2.6 in multi-version
             QStringList appInfoList = appInfo.split("/");
             paramOption.appId = appInfo;
             if (appInfoList.size() > 1) {
                 paramOption.appId = appInfoList.at(0);
                 paramOption.version = appInfoList.at(1);
             }
             paramOption.nodbus = parser.isSet(optNoDbus);
             paramOption.repoPoint = repoType;
             linglong::service::Reply reply;
             if (paramOption.nodbus) {
                 reply = noDbusPackageManager->Uninstall(paramOption);
                 if (reply.code != STATUS_CODE(pkg_uninstall_success)) {
                     qInfo().noquote() << "message: " << reply.message << ", errcode:" << reply.code;
                     return -1;
                 } else {
                     qInfo().noquote() << "uninstall " << appInfo << " success";
                 }
                 return 0;
             }
             dbusReply = packageManager.Uninstall(paramOption);
             dbusReply.waitForFinished();
             reply = dbusReply.value();

             if (reply.code != STATUS_CODE(pkg_uninstall_success)) {
                 qCritical().noquote() << "message:" << reply.message << ", errcode:" << reply.code;
                 return -1;
             }
             qInfo().noquote() << "message:" << reply.message;
             return 0;
         }},
        {"list", // 查询已安装玲珑包
         [&](QCommandLineParser &parser) -> int {
             auto optType = QCommandLineOption("type", "query installed app", "--type=installed", "installed");
             parser.clearPositionalArguments();
             parser.addPositionalArgument("list", "show installed application", "list");
             parser.addOption(optType);
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto optPara = parser.value(optType);
             if ("installed" != optPara) {
                 parser.showHelp(-1);
                 return -1;
             }
             auto repoType = parser.value(optRepoPoint);
             if (!repoType.isEmpty() && "flatpak" != repoType) {
                 parser.showHelp(-1);
                 return -1;
             }

             linglong::service::QueryParamOption paramOption;
             paramOption.appId = optPara;
             paramOption.repoPoint = repoType;
             linglong::package::AppMetaInfoList appMetaInfoList;
             QDBusPendingReply<linglong::service::QueryReply> dbusReply = packageManager.Query(paramOption);
             // 默认超时时间为25s
             dbusReply.waitForFinished();
             linglong::service::QueryReply reply = dbusReply.value();
             linglong::util::getAppMetaInfoListByJson(reply.result, appMetaInfoList);
             if (1 == appMetaInfoList.size() && "flatpaklist" == appMetaInfoList.at(0)->appId) {
                 printFlatpakAppInfo(appMetaInfoList);
             } else {
                 printAppInfo(appMetaInfoList);
             }
             return 0;
         }},
    };

    if (subcommandMap.contains(command)) {
        auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
