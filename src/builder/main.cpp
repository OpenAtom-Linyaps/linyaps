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
#include <QRegExp>

#include "builder/project.h"
#include "builder/builder.h"
#include "builder/bst_builder.h"

using namespace linglong;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    qJsonRegister<linglong::builder::Project>();
    qJsonRegister<linglong::builder::Variables>();

    QCommandLineParser parser;

    parser.addHelpOption();

    QStringList subCommandList = {
        "create",
        "build",
        "export",
        "push",
    };

    parser.addPositionalArgument("subcommand", subCommandList.join("\n"), "subcommand [sub-option]");

    parser.parse(QCoreApplication::arguments());

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    builder::Builder *builder = new builder::BstBuilder();

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        {"create",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("create", "create build template project", "create");
             parser.addPositionalArgument("name", "project name", "<org.deepin.demo>");

             parser.process(app);

             auto args = parser.positionalArguments();
             auto projectName = args.value(1);

             if (projectName.isEmpty()) {
                 parser.showHelp(-1);
             }

             // TODO: extract an empty buildstream project from qrc file
             auto result = builder->create(projectName);

             if (!result.success()) {
                 qDebug() << result;
             }

             return result.code();
         }},
        {"build",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("build", "build project", "build");

             parser.process(app);

             // TODO: build current project
             auto result = builder->build();
             if (!result.success()) {
                 qDebug() << result;
             }

             return result.code();
         }},
        {"export",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("export", "export build result to uab bundle", "export");
             parser.addPositionalArgument("filename", "bundle file name", "<filename>");

             parser.process(app);

             auto outputFilepath = parser.positionalArguments().value(1);

             if (outputFilepath.isEmpty()) {
                 parser.showHelp(-1);
             }

             // TODO: export build result to bundle
             auto result = builder->exportBundle(outputFilepath);
             if (!result.success()) {
                 qCritical() << result;
             }

             return result.code();
         }},
        {"push",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();
             parser.addPositionalArgument("push", "push build result to repo", "push");

             //  auto optRepoURL = QCommandLineOption("repo-url", "repo url", "repo-url");
             //  parser.addOption(optRepoURL);
             parser.addPositionalArgument("filePath", "bundle file path", "<filePath>");
             auto optForce = QCommandLineOption("force", "force push true or false", "");
             parser.addOption(optForce);

             parser.process(app);

             auto args = parser.positionalArguments();

             auto outputFilepath = parser.positionalArguments().value(1);

             // auto repoURL = parser.value(optRepoURL);
             auto force = parser.isSet(optForce);

             // TODO: push build result to repo
             // auto result = builder->push(repoURL, force);
             auto result = builder->push(outputFilepath, force);
             if (!result.success()) {
                 qDebug() << result;
             }

             return result.code();
         }},

    };

    if (subcommandMap.contains(command)) {
        auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
