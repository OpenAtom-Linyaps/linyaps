/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
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

    auto optVerbose = QCommandLineOption("verbose", "show detail log", "");
    parser.addOption(optVerbose);
    parser.addHelpOption();

    QStringList subCommandList = {
        "create",
        "build",
        "export",
        "push",
    };

    parser.addPositionalArgument("subcommand", subCommandList.join("\n"), "subcommand [sub-option]");

    parser.parse(QCoreApplication::arguments());

    if (parser.isSet(optVerbose)) {
        qputenv("QT_LOGGING_RULES", "*=true");
    }


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
             parser.addPositionalArgument(
                 "filename", "bundle file name , if filename is empty,export default format bundle", "[filename]");
             parser.process(app);

             auto outputFilepath = parser.positionalArguments().value(1);
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
