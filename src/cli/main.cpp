/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QMap>

#include <DLog>
#include <cmd/cmd.h>

#include "module/package/package.h"

#include "service/impl/json_register_inc.h"


#include "package_manager.h"

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
    parser.addPositionalArgument("subcommand", "run\nps\nkill\ndownload\ninstall\nuninstall\nupdate\nquery\nbuild\nrepo", "subcommand [sub-option]");
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

             pm.Start({appID});
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
             //  ll-cmd download org.deepin.calculator -d ./test 无-d 参数默认当前路径
             auto optDownload = QCommandLineOption("d", "dest path to save app", "dest path to save app", curPath);

             parser.clearPositionalArguments();
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");
             parser.addOption(optDownload);
             parser.process(app);
             auto args = parser.positionalArguments();
             //第一个参数为命令字
             auto appID = args.value(1);
             auto savePath = parser.value(optDownload);
             pm.Download({appID}, savePath);
             // OperateRet ret = reply.value();
             // if (v.canConvert<OperateRet>()) {
             // OperateRet ret = v.value<OperateRet>();
             // std::cout << ret.retCode << "," << ret.retInfo.toStdString() << std::endl;
             //  get progress
             // }
             return -1;
         }},
        {"install", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install an application", "install");
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");

             parser.process(app);

             auto args = parser.positionalArguments();
             auto appID = args.value(1);

             auto jobID = pm.Install({appID});
             // get progress
             return -1;
         }},
        {"repo", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ls", "show repo content", "ls");
             parser.addPositionalArgument("repo", "repo", "repo");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             QString subCommand = args.isEmpty() ? QString() : args.first();
             auto repoID = args.value(1);

             // TODO: show repo result
             //        repo::Manager m;
             //        return m.ls(repoID);
             return -1;
         }},
        {"list", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("list", "show install application", "list");
             parser.addPositionalArgument("appid", "app id", "com.deepin.demo");

             auto optOutputFormat = QCommandLineOption("output-format", "json/console", "console");
             parser.addOption(optOutputFormat);

             parser.process(app);

             QString subCommand = args.isEmpty() ? QString() : args.first();
             auto appid = args.value(1);

             auto outputFormat = parser.value(optOutputFormat);
             auto ret_msg = pm.QDbusRetInfo({appid}).value();
             for (auto const &it : ret_msg) {
                 qInfo() << "id:\t" << it->appid;
                 qInfo() << "name:\t" << it->appname;
                 qInfo() << "version:\t" << it->version;
             }
             // showContainer(ret_msg, outputFormat);
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
