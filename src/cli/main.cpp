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

#include "cmd/cmd.h"
#include "module/package/package.h"
#include "module/util/package_manager_param.h"
#include "service/impl/json_register_inc.h"
#include "service/impl/package_manager.h"
#include "service/impl/package_manager_option.h"
#include "package_manager.h"
#include "module/runtime/runtime.h"
#include "module/util/xdg.h"
#include "module/util/env.h"
#include "module/util/log_handler.h"

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
        qInfo("%-32s%-32s%-16s%-12s%-s", qUtf8Printable("appId"), qUtf8Printable("name"), qUtf8Printable("version"),
              qUtf8Printable("arch"), qUtf8Printable("description"));
        for (auto const &it : appMetaInfoList) {
            QString simpleDescription = it->description.trimmed();
            if (simpleDescription.length() > 56) {
                simpleDescription = it->description.trimmed().left(53) + "...";
            }
            QString appId = it->appId.trimmed();
            if (appId.length() > 32) {
                appId = it->appId.trimmed().left(29) + "...";
            }
            QString name = it->name.trimmed();
            if (name.length() > 32) {
                name = it->name.trimmed().left(29) + "...";
            }
            int count = getUnicodeNum(name);
            qInfo().noquote() << QString("%1%2%3%4%5")
                                     .arg(appId, -32, QLatin1Char(' '))
                                     .arg(name, count - 32, QLatin1Char(' '))
                                     .arg(it->version.trimmed(), -16, QLatin1Char(' '))
                                     .arg(it->arch.trimmed(), -12, QLatin1Char(' '))
                                     .arg(simpleDescription, -56, QLatin1Char(' '));
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

    ComDeepinLinglongPackageManagerInterface packageManager("com.deepin.linglong.AppManager", "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    checkAndStartService(packageManager);
    PackageManager *noDbusPackageManager = PackageManager::instance();
    auto optNoDbus = QCommandLineOption("nodbus", "execute cmd directly, not via dbus", "");
    parser.addOption(optNoDbus);
    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        {"run",     // 启动玲珑应用
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
             if ((!repoType.isEmpty() && repoType != "flatpak")) {
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

             // appId format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appId.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::kKeyVersion, appInfoList.at(1));
             }
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyRepoPoint, repoType);
             }

             if (!exec.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyExec, exec);
             }

             // 获取用户环境变量
             QStringList envList = getUserEnv(linglong::util::envList);
             if (!envList.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyEnvlist, envList.join(","));
             }

             // 判断是否设置了no-proxy参数
             if (parser.isSet(optNoProxy)) {
                 paramMap.insert(linglong::util::kKeyNoProxy, "");
             }

             if (!parser.isSet(optNoProxy)) {
                 // FIX to do only deal with session bus
                 paramMap.insert(linglong::util::kKeyBusType, "session");
                 auto nameFilter = parser.value(optNameFilter);
                 if (!nameFilter.isEmpty()) {
                     paramMap.insert(linglong::util::kKeyFilterName, nameFilter);
                 }
                 auto pathFilter = parser.value(optPathFilter);
                 if (!pathFilter.isEmpty()) {
                     paramMap.insert(linglong::util::kKeyFilterPath, pathFilter);
                 }
                 auto interfaceFilter = parser.value(optInterfaceFilter);
                 if (!interfaceFilter.isEmpty()) {
                     paramMap.insert(linglong::util::kKeyFilterIface, interfaceFilter);
                 }
             }

             QDBusPendingReply<RetMessageList> reply = packageManager.Start(appInfoList.at(0), paramMap);
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             return 0;
         }},
        {"exec",    // 进入玲珑沙箱
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("containerId", "container id", "aebbe2f455cf443f89d5c92f36d154dd");
             parser.addPositionalArgument("exec", "exec command in container", "/bin/bash");
             parser.process(app);

             auto containerId = parser.positionalArguments().value(1);
             if (containerId.isEmpty()) {
                 parser.showHelp();
             }

             auto cmd = parser.positionalArguments().value(2);
             if (cmd.isEmpty()) {
                 parser.showHelp();
             }

             auto pid = containerId.toInt();

             return namespaceEnter(pid, QStringList {cmd});
         }},
        {"ps",      // 查看玲珑沙箱进程
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ps", "show running applications", "ps");

             auto optOutputFormat = QCommandLineOption("output-format", "json/console", "console");
             parser.addOption(optOutputFormat);

             parser.process(app);

             auto outputFormat = parser.value(optOutputFormat);
             auto containerList = packageManager.ListContainer().value();
             showContainer(containerList, outputFormat);
             return 0;
         }},
        {"kill",    // 关闭玲珑沙箱
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("kill", "kill container with id", "kill");
             parser.addPositionalArgument("container-id", "container id", "");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             auto containerId = args.value(1);

             // TODO: show kill result
             QDBusPendingReply<RetMessageList> reply = packageManager.Stop(containerId);
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             return 0;
         }},
        {"download",    // TODO: download命令当前没用到
         [&](QCommandLineParser &parser) -> int {
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
             auto appId = args.value(1);
             auto savePath = parser.value(optDownload);
             QFileInfo dstfs(savePath);

             packageManager.setTimeout(1000 * 60 * 60 * 24);
             QDBusPendingReply<RetMessageList> reply = packageManager.Download({appId}, dstfs.absoluteFilePath());
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             return 0;
         }},
        {"install",     // 下载玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install an application", "install");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto args = parser.positionalArguments();
             auto appId = args.value(1);
             auto repoType = parser.value(optRepoPoint);
             if (appId.isEmpty() || (!repoType.isEmpty() && repoType != "flatpak")) {
                 parser.showHelp(-1);
                 return -1;
             }
             // 设置 24 h超时
             packageManager.setTimeout(1000 * 60 * 60 * 24);
             QDBusPendingReply<RetMessageList> reply;

             // appId format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appId.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::kKeyVersion, appInfoList.at(1));
             }
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyRepoPoint, repoType);
             }

             auto noDbus = parser.isSet(optNoDbus);
             if (noDbus) {
                 RetMessageList retMessageList = noDbusPackageManager->Install({appInfoList.at(0)}, paramMap);
                 if (retMessageList.size() > 0) {
                     auto it = retMessageList.at(0);
                     qInfo().noquote() << "message: " << it->message;
                     if (!it->state) {
                         return -1;
                     }
                     return 0;
                 }
             }
             reply = packageManager.Install({appInfoList.at(0)}, paramMap);

             qInfo().noquote() << "install " << appId << ", please wait a few minutes...";
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             if (reply.isError()) {
                 qCritical().noquote() << "install: " << appId << " timeout";
                 return -1;
             }
             return 0;
         }},
        {"update",  // 更新玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("update", "update an application", "update");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             parser.process(app);
             auto args = parser.positionalArguments();
             auto appId = args.value(1);
             if (appId.isEmpty()) {
                 parser.showHelp(-1);
                 return -1;
             }
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appId.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::kKeyVersion, appInfoList.at(1));
             }
             packageManager.setTimeout(1000 * 60 * 60 * 24);
             QDBusPendingReply<RetMessageList> reply = packageManager.Update({appInfoList.at(0)}, paramMap);
             qInfo().noquote() << "update " << appId << ", please wait a few minutes...";
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             if (reply.isError()) {
                 qCritical().noquote() << "update: " << appId << " timeout";
                 return -1;
             }
             return 0;
         }},
        {"query",   // 查询玲珑包
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
             if (!repoType.isEmpty() && repoType != "flatpak") {
                 parser.showHelp(-1);
                 return -1;
             }
             QMap<QString, QString> paramMap;
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyRepoPoint, repoType);
             }
             auto noCache = parser.isSet(optNoCache);
             if (noCache) {
                 paramMap.insert(linglong::util::kKeyNoCache, "");
             }
             auto args = parser.positionalArguments();
             auto appId = args.value(1);
             QDBusPendingReply<linglong::package::AppMetaInfoList> reply = packageManager.Query({appId}, paramMap);
             reply.waitForFinished();
             linglong::package::AppMetaInfoList appMetaInfoList = reply.value();
             if (appMetaInfoList.size() == 1 && appMetaInfoList.at(0)->appId == "flatpakquery") {
                 printFlatpakAppInfo(appMetaInfoList);
             } else {
                 printAppInfo(appMetaInfoList);
             }
             return 0;
         }},
        {"uninstall",   // 卸载玲珑包
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("uninstall", "uninstall an application", "uninstall");
             parser.addPositionalArgument("appId", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
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
             QDBusPendingReply<RetMessageList> reply;

             // appId format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appInfo.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::kKeyVersion, appInfoList.at(1));
             }
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyRepoPoint, repoType);
             }
             auto noDbus = parser.isSet(optNoDbus);
             if (noDbus) {
                 RetMessageList retMessageList = noDbusPackageManager->Uninstall({appInfoList.at(0)}, paramMap);
                 if (retMessageList.size() > 0) {
                     auto it = retMessageList.at(0);
                     qInfo().noquote() << "message: " << it->message;
                     if (!it->state) {
                         return -1;
                     }
                     return 0;
                 }
             }
             reply = packageManager.Uninstall({appInfoList.at(0)}, paramMap);
             reply.waitForFinished();
             RetMessageList retMessageList = reply.value();
             if (retMessageList.size() > 0) {
                 auto it = retMessageList.at(0);
                 QString prompt = "message: " + it->message;
                 if (!it->state) {
                     qCritical().noquote() << prompt << ", errcode:" << it->code;
                     return -1;
                 }
                 qInfo().noquote() << prompt;
             }
             return 0;
         }},
        {"list",    // 查询已安装玲珑包
         [&](QCommandLineParser &parser) -> int {
             auto optType = QCommandLineOption("type", "query installed app", "--type=installed", "installed");
             parser.clearPositionalArguments();
             parser.addPositionalArgument("list", "show installed application", "list");
             parser.addOption(optType);
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto optPara = parser.value(optType);
             if (optPara != "installed") {
                 parser.showHelp(-1);
                 return -1;
             }
             auto repoType = parser.value(optRepoPoint);
             if (!repoType.isEmpty() && repoType != "flatpak") {
                 parser.showHelp(-1);
                 return -1;
             }
             QMap<QString, QString> paramMap;
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::kKeyRepoPoint, repoType);
             }
             QDBusPendingReply<linglong::package::AppMetaInfoList> reply = packageManager.Query({optPara}, paramMap);
             // 默认超时时间为25s
             reply.waitForFinished();
             linglong::package::AppMetaInfoList appMetaInfoList = reply.value();
             if (appMetaInfoList.size() == 1 && "flatpaklist" == appMetaInfoList.at(0)->appId) {
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
