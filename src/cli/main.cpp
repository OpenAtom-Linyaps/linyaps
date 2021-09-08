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

#include "module/package/package.h"
#include "module/runtime/container.h"
#include "package_manager.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;

    parser.addHelpOption();
    parser.addPositionalArgument("subcommand", "run\nbuild\nps\nkill\ninstall\nrepo", "subcommand [sub-option]");
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

             // TODO
             //        QFile f(configPath);
             //        f.open(QIODevice::ReadOnly);
             //        auto r = linglong::fromString(f.readAll().toStdString());
             //        f.close();
             //
             //        if (!exec.isEmpty()) {
             //          r.process.args = {exec.toStdString()};
             //        }
             //
             //        runtime::Application ogApp(appID, r);
             //        return ogApp.start();
             return -1;
         }},
        {"ps", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ps", "show running applications", "ps");

             auto optOutputFormat = QCommandLineOption("output-format", "json/console", "console");
             parser.addOption(optOutputFormat);

             parser.process(app);

             auto outputFormat = parser.value(optOutputFormat);

             // TODO: show ps result
             //        return runtime::Manager::ps(outputFormat);
             return -1;
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
        {"install", [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install an application", "install");
             parser.addPositionalArgument("app-id", "app id", "com.deepin.demo");

             parser.process(app);

             auto args = parser.positionalArguments();
             auto appID = args.value(1);

             auto jobID = pm.Install({appID});
             // get progress
             return 0;
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
    };

    if (subcommandMap.contains(command)) {
        auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
