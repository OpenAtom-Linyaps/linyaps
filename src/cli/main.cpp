/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iomanip>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QMap>
#include <DLog>

#include "cmd/cmd.h"
#include "module/package/package.h"
#include "module/util/package_manager_param.h"
#include "service/impl/json_register_inc.h"
#include "package_manager.h"

void printFlatpakAppInfo(AppMetaInfoList retMsg)
{
    if (retMsg.size() > 0) {
        if (retMsg.at(0)->appId == "flatpaklist") {
            std::cout << std::setiosflags(std::ios::left) << std::setw(48) << "Description" << std::setw(16)
                      << "Application" << std::setw(16) << "Version" << std::setw(12) << "Branch" << std::setw(12)
                      << "Arch" << std::setw(12) << "Origin" << std::setw(12) << "Installation" << std::endl;
        } else {
            std::cout << std::setiosflags(std::ios::left) << std::setw(72) << "Description" << std::setw(16)
                      << "Application" << std::setw(16) << "Version" << std::setw(12) << "Branch" << std::setw(12)
                      << "Remotes" << std::endl;
        }
        QString ret = retMsg.at(0)->description;
        QStringList strList = ret.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
        for (int i = 0; i < strList.size(); ++i) {
            std::cout << strList[i].simplified().toStdString() << std::endl;
        }
    }
}

void printAppInfo(AppMetaInfoList retMsg)
{
    if (retMsg.size() > 0) {
        std::cout << std::setiosflags(std::ios::left) << std::setw(24) << "id" << std::setw(16)
                  << "name" << std::setw(16) << "version" << std::setw(12) << "arch"
                  << "description" << std::endl;
        // 最长显示字符数
        const int maxDisSize = 50;
        for (auto const &it : retMsg) {
            QString simpleDescription = it->description;
            if (it->description.length() > maxDisSize) {
                simpleDescription = it->description.left(maxDisSize) + "...";
            }
            std::cout << std::setiosflags(std::ios::left) << std::setw(24) << it->appId.toStdString()
                      << std::setw(16) << it->name.toStdString() << std::setw(16) << it->version.toStdString()
                      << std::setw(12) << it->arch.toStdString() << simpleDescription.toStdString() << std::endl;
        }
    } else {
        std::cout << "app not found in repo" << std::endl;
    }
}

void checkAndStartService(ComDeepinLinglongPackageManagerInterface &pm)
{
    const auto kStatusActive = "active";
    QDBusReply<QString> status = pm.Status();
    // FIXME: should use more precision to check status
    if (kStatusActive != status.value()) {
        QProcess process;
        process.setProgram("ll-service");
        process.setStandardOutputFile("/dev/null");
        process.setStandardErrorFile("/dev/null");
        process.setArguments({});
        process.startDetached();
    }

    for (int i = 0; i < 10; ++i) {
        status = pm.Status();
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

    Dtk::Core::DLogManager::registerConsoleAppender();
    Dtk::Core::DLogManager::registerFileAppender();

    // register qdbus type
    RegisterDbusType();

    QCommandLineParser parser;
    parser.addHelpOption();
    QStringList subCommandList = {
        "run", "ps", "exec", "kill", "download", "install", "uninstall", "update", "query", "list",
    };

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

    ComDeepinLinglongPackageManagerInterface pm("com.deepin.linglong.AppManager",
                                                "/com/deepin/linglong/PackageManager",
                                                QDBusConnection::sessionBus());
    checkAndStartService(pm);

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        {"run", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("run", "run application", "run");
             parser.addPositionalArgument("appID", "application id", "com.deepin.demo");

             auto optExec = QCommandLineOption("exec", "run exec", "/bin/bash");
             parser.addOption(optExec);
             parser.process(app);

             auto appID = parser.positionalArguments().value(1);
             if (appID.isEmpty()) {
                 parser.showHelp();
             }
             auto exec = parser.value(optExec);

             // appID format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appID.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::KEY_VERSION, appInfoList.at(1));
             }
             pm.Start(appInfoList.at(0), paramMap);

             // TODO
             //        QFile f(configPath);
             //        f.open(QIODevice::ReadOnly);
             //        auto r = linglong::fromString(f.readAll().toStdString());
             //        f.close();
             //
             //  if (!exec.isEmpty()) {
             //      r.process.args = {exec.toStdString()};
             //  }

             //  runtime::Application ogApp(appID, r);
             //  return ogApp.start();
             return -1;
         }},
        {"exec", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("containerID", "container id", "aebbe2f455cf443f89d5c92f36d154dd");
             parser.addPositionalArgument("exec", "exec command in container", "/bin/bash");
             parser.process(app);

             auto containerID = parser.positionalArguments().value(1);
             if (containerID.isEmpty()) {
                 parser.showHelp();
             }

             auto cmd = parser.positionalArguments().value(2);
             if (cmd.isEmpty()) {
                 parser.showHelp();
             }

             auto pid = containerID.toInt();

             return namespaceEnter(pid, QStringList {cmd});
         }},
        {"ps", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ps", "show running applications", "ps");

             auto optOutputFormat = QCommandLineOption("output-format", "json/console", "console");
             parser.addOption(optOutputFormat);

             parser.process(app);

             auto outputFormat = parser.value(optOutputFormat);
             auto containerList = pm.ListContainer().value();
             showContainer(containerList, outputFormat);
             return 0;
         }},
        {"kill", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("kill", "kill container with id", "kill");
             parser.addPositionalArgument("container-id", "container id", "");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             auto containerID = args.value(1);

             // TODO: show kill result
             //        return runtime::Manager::kill(containerID);
             return -1;
         }},
        {"download", [&](QCommandLineParser &parser) -> int {
             QString curPath = QDir::currentPath();
             // qDebug() << curPath;
             //  ll-cli download org.deepin.calculator -d ./test 无-d 参数默认当前路径
             auto optDownload = QCommandLineOption("d", "dest path to save app", "dest path to save app", curPath);

             parser.clearPositionalArguments();
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");
             parser.addOption(optDownload);
             parser.process(app);
             auto args = parser.positionalArguments();
             //第一个参数为命令字
             auto appID = args.value(1);
             auto savePath = parser.value(optDownload);
             QFileInfo dstfs(savePath);
             // 设置 10 分钟超时 to do
             pm.setTimeout(1000 * 60 * 10);
             QDBusPendingReply<RetMessageList> reply = pm.Download({appID}, dstfs.absoluteFilePath());
             reply.waitForFinished();
             RetMessageList ret_msg = reply.value();
             if (ret_msg.size() > 0) {
                 auto it = ret_msg.at(0);
                 std::cout << "message: " << it->message.toStdString();
                 if (!it->state) {
                     std::cout << ", errcode:" << it->code << std::endl;
                     return -1;
                 }
                 std::cout << std::endl;
             }
             return 0;
         }},
        {"install", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install an application", "install");
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto args = parser.positionalArguments();
             auto appID = args.value(1);
             auto repoType = parser.value(optRepoPoint);
             if (appID.isEmpty() || (!repoType.isEmpty() && repoType != "flatpak")) {
                 parser.showHelp(-1);
                 return -1;
             }
             // 设置 10 分钟超时 to do
             pm.setTimeout(1000 * 60 * 10);
             QDBusPendingReply<RetMessageList> reply;

             // appID format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appID.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::KEY_VERSION, appInfoList.at(1));
             }
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::KEY_REPO_POINT, repoType);
             }
             reply = pm.Install({appInfoList.at(0)}, paramMap);

             std::cout << "install " << appID.toStdString() << ", please wait a few minutes..." << std::endl;
             reply.waitForFinished();
             RetMessageList ret_msg = reply.value();
             if (ret_msg.size() > 0) {
                 auto it = ret_msg.at(0);
                 std::cout << "message: " << it->message.toStdString();
                 if (!it->state) {
                     std::cout << ", errcode:" << it->code << std::endl;
                     return -1;
                 }
                 std::cout << std::endl;
             }
             return 0;
         }},
        {"query", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("query", "query app info", "query");
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");
             auto optRepoPoint = QCommandLineOption("repo-point", "app repo type to use", "--repo-point=flatpak", "");
             parser.addOption(optRepoPoint);
             parser.process(app);
             auto repoType = parser.value(optRepoPoint);
             if (!repoType.isEmpty() && repoType != "flatpak") {
                 parser.showHelp(-1);
                 return -1;
             }
             QMap<QString, QString> paramMap;
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::KEY_REPO_POINT, repoType);
             }
             auto args = parser.positionalArguments();
             auto appID = args.value(1);
             QDBusPendingReply<AppMetaInfoList> reply = pm.Query({appID}, paramMap);
             reply.waitForFinished();
             AppMetaInfoList retMsg = reply.value();
             if (retMsg.size() == 1 && retMsg.at(0)->appId == "flatpakquery") {
                 printFlatpakAppInfo(retMsg);
             } else {
                 printAppInfo(retMsg);
             }
             return 0;
         }},
        {"uninstall", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("uninstall", "uninstall an application", "uninstall");
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");
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
             // 设置 10 分钟超时 to do
             pm.setTimeout(1000 * 60 * 10);
             QDBusPendingReply<RetMessageList> reply;

             // appID format: org.deepin.calculator/1.2.6 in multi-version
             QMap<QString, QString> paramMap;
             QStringList appInfoList = appInfo.split("/");
             if (appInfoList.size() > 1) {
                 paramMap.insert(linglong::util::KEY_VERSION, appInfoList.at(1));
             }
             if (!repoType.isEmpty()) {
                 paramMap.insert(linglong::util::KEY_REPO_POINT, repoType);
             }
             reply = pm.Uninstall({appInfoList.at(0)}, paramMap);
             reply.waitForFinished();
             RetMessageList retMsg = reply.value();
             if (retMsg.size() > 0) {
                 auto it = retMsg.at(0);
                 std::cout << "message: " << it->message.toStdString();
                 if (!it->state) {
                     std::cout << ", errcode:" << it->code << std::endl;
                     return -1;
                 }
                 std::cout << std::endl;
             }
             return 0;
         }},
        {"list", [&](QCommandLineParser &parser) -> int {
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
                 paramMap.insert(linglong::util::KEY_REPO_POINT, repoType);
             }
             QDBusPendingReply<AppMetaInfoList> reply = pm.Query({optPara}, paramMap);
             // 默认超时时间为25s
             reply.waitForFinished();
             AppMetaInfoList retMsg = reply.value();
             if (retMsg.size() == 1 && retMsg.at(0)->appId == "flatpaklist") {
                 printFlatpakAppInfo(retMsg);
             } else {
                 printAppInfo(retMsg);
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
