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
#include <QRegExp>

#include "module/package/package.h"

// using Package;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;

    parser.addHelpOption();
    parser.addPositionalArgument("subcommand", "build\npackage", "subcommand [sub-option]");
    // TODO(SE): for debug now
    auto optDefaultConfig = QCommandLineOption("default-config", "default config json filepath", "");
    parser.addOption(optDefaultConfig);

    parser.parse(QCoreApplication::arguments());

    auto configPath = parser.value(optDefaultConfig);
    if (configPath.isEmpty()) {
        configPath = ":/config.json";
    }

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        {"build",
         [&](QCommandLineParser &parser) -> int {
             parser.addPositionalArgument("build", "build uap package", "build");
             parser.addPositionalArgument("config", "config json", "config.json");
             parser.addPositionalArgument("data-dir", "data dir", "");

             parser.process(app);

             auto args = parser.positionalArguments();
             // TODO(SE):

             if (args.size() == 3) {
                 Package create_package;
                 create_package.InitUap(args.at(1), args.at(2));
                 create_package.MakeUap();
                 return 0;
             }
             if (args.size() == 2) {
                 Package create_package;
                 create_package.InitUap(args.at(1));
                 create_package.MakeUap();

                 return 0;
             }
             qInfo() << args;
             qInfo() << "err! input config.json and data-dir";
             return -1;
         }},
        {"extract",
         [&](QCommandLineParser &parser) -> int {
             parser.addPositionalArgument("extract", "extract uap package", "extract");
             parser.addPositionalArgument("uap-package", "uap package", "");
             parser.addPositionalArgument("dir", "dir", "");

             parser.process(app);

             auto args = parser.positionalArguments();
             // TODO(SE):

             if (args.size() == 3) {
                 Package create_package;
                 create_package.Extract(args.at(1), args.at(2));
                 return 0;
             }

             qInfo() << args;
             qInfo() << "err! input uap-package and dir";
             return -1;
         }},
        {"check",
         [&](QCommandLineParser &parser) -> int {
             parser.addPositionalArgument("check", "check uap package", "check");
             parser.addPositionalArgument("dirpath", "dir path", "");

             parser.process(app);

             auto args = parser.positionalArguments();
             // TODO(SE):

             if (args.size() == 2) {
                 Package create_package;
                 create_package.Check(args.at(1));
                 return 0;
             }

             qInfo() << args;
             qInfo() << "err! input  dirpath";
             return -1;
         }},
        {"install",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("install", "install uap", "install");
             parser.addPositionalArgument("list", "uap file list", "list");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             QString subCommand = args.isEmpty() ? QString() : args.first();

             QStringList uap_list = args.filter(QRegExp("^*.uap$", Qt::CaseInsensitive));

             qInfo() << uap_list;
             if (!(uap_list.size() > 0)) {
                 qInfo() << "err:input uap file!";
                 return -1;
             }
             // install uap package
             for (auto it : uap_list) {
                 Package pkg;
                 pkg.InitUapFromFile(it);
             }

             // TODO(SE):
             return 0;
         }},
        {"package",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("ls", "show repo content", "ls");
             parser.addPositionalArgument("repo", "repo", "repo");

             parser.process(app);

             QStringList args = parser.positionalArguments();

             QString subCommand = args.isEmpty() ? QString() : args.first();
             auto repoID = args.value(1);

             // TODO(SE):
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
