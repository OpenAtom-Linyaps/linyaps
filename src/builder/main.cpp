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

#include <fstream>

#include "builder/project.h"
#include "builder/builder.h"
#include "builder/bst_builder.h"
#include "builder/linglong_builder.h"
#include "builder/builder_config.h"
#include "module/package/package.h"
#include "module/runtime/oci.h"
#include "module/runtime/runtime.h"
#include "module/util/yaml.h"

using namespace linglong;

static void qJsonRegisterAll()
{
    builder::registerAllMetaType();
    package::registerAllMetaType();
    runtime::registerAllMetaType();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("deepin");

    qJsonRegisterAll();

    builder::BuilderConfig::instance()->setProjectRoot(QDir::currentPath());

    QCommandLineParser parser;

    auto optVerbose = QCommandLineOption({"v", "verbose"}, "show detail log", "");
    parser.addOption(optVerbose);
    parser.addHelpOption();

    QStringList subCommandList = {"create", "build", "run", "export", "push"};

    parser.addPositionalArgument("subcommand", subCommandList.join("\n"), "subcommand [sub-option]");

    parser.parse(QCoreApplication::arguments());

    if (parser.isSet(optVerbose)) {
        qputenv("QT_LOGGING_RULES", "*=true");
    }

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    builder::BstBuilder _bb;
    builder::LinglongBuilder _llb;
    builder::Builder *builder = &_llb;

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

             auto result = builder->create(projectName);

             if (!result.success()) {
                 qDebug() << result;
             }

             return result.code();
         }},
        {"build",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();

             auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
	     auto pkgVersion = QCommandLineOption("pversion", "set pacakge version", "pacakge version");
             auto srcVersion = QCommandLineOption("sversion", "set source version", "source version");
             auto srcCommit = QCommandLineOption("commit", "set commit refs", "source commit");
             parser.addOption(execVerbose);
             parser.addOption(pkgVersion);
             parser.addOption(srcVersion);
             parser.addOption(srcCommit);

             parser.addPositionalArgument("build", "build project", "build");

             parser.process(app);

             if (parser.isSet(execVerbose)) {
                 builder::BuilderConfig::instance()->setExec(parser.value(execVerbose));
             }
            // config linglong.yaml before build if necessary
             if (parser.isSet(pkgVersion) || parser.isSet(srcVersion) || parser.isSet(srcCommit)) {

                 auto projectConfigPath = QStringList {builder::BuilderConfig::instance()->projectRoot(), "linglong.yaml"}.join("/");

                 QScopedPointer<builder::Project> project(formYaml<builder::Project>(YAML::LoadFile(projectConfigPath.toStdString())));
                 
                 auto node = YAML::LoadFile(projectConfigPath.toStdString());

                 node["package"]["version"] = parser.value(pkgVersion).isEmpty()
                                                 ? project->package->version.toStdString()
                                                 : parser.value(pkgVersion).toStdString();

                 node["source"]["version"] = parser.value(srcVersion).isEmpty() 
                                                 ? project->source->version.toStdString()
                                                 : parser.value(srcVersion).toStdString();

                 node["source"]["commit"] = parser.value(srcCommit).isEmpty()
                                                 ? project->source->commit.toStdString()
                                                 : parser.value(srcCommit).toStdString();
                 // fixme: use qt file stream
                 std::ofstream fout(projectConfigPath.toStdString());
                 fout << node;
             }
             auto result = builder->build();
             if (!result.success()) {
                 qCritical() << result;
             }

             return result.code();
         }},
        {"run",
         [&](QCommandLineParser &parser) -> int {
             parser.clearPositionalArguments();

             auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
             parser.addOption(execVerbose);

             parser.addPositionalArgument("run", "run project", "build");

             parser.process(app);

             if (parser.isSet(execVerbose)) {
                 builder::BuilderConfig::instance()->setExec(parser.value(execVerbose));
             }

             auto result = builder->run();
             if (!result.success()) {
                 qCritical() << result;
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

             // auto result = builder->push(repoURL, force);
             auto result = builder->push(outputFilepath, force);
             if (!result.success()) {
                 qCritical() << result;
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
