/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/builder.h"
#include "linglong/builder/builder_config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/builder/project.h"
#include "linglong/cli/printer.h"
#include "linglong/package/package.h"
#include "linglong/repo/repo.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMap>
#include <QRegExp>

#include <fstream>

int main(int argc, char **argv)
{
    qputenv("QT_LOGGING_RULES", "*=false");
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "-v") == 0) {
            qputenv("QT_LOGGING_RULES", "*=true");
            break;
        }
    }
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    linglong::builder::BuilderConfig::instance()->setProjectRoot(QDir::currentPath());

    QCommandLineParser parser;

    auto optVerbose = QCommandLineOption({ "v", "verbose" }, "show detail log", "");
    parser.addOptions({ optVerbose });
    parser.addHelpOption();

    QStringList subCommandList = { "create", "build", "run", "export", "push", "generate" };

    parser.addPositionalArgument("subcommand",
                                 subCommandList.join("\n"),
                                 "subcommand [sub-option]");

    parser.parse(QCoreApplication::arguments());

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    QNetworkAccessManager networkAccessManager;

    linglong::api::client::ClientApi api;
    api.setTimeOut(10 * 60 * 1000);
    api.setNetworkAccessManager(&networkAccessManager);
    api.setNewServerForAllOperations(
      linglong::builder::BuilderConfig::instance()->remoteRepoEndpoint);

    auto config = linglong::repo::config::ConfigV1{
        linglong::builder::BuilderConfig::instance()->remoteRepoName.toStdString(),
        { { linglong::builder::BuilderConfig::instance()->remoteRepoName.toStdString(),
            linglong::builder::BuilderConfig::instance()->remoteRepoEndpoint.toStdString() } },
        1
    };

    linglong::repo::OSTreeRepo ostree(linglong::builder::BuilderConfig::instance()->repoPath(),
                                      config,
                                      api);
    linglong::cli::Printer printer;
    linglong::builder::LinglongBuilder builder(ostree, printer);

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        { "generate",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("generate",
                                           "generate yaml config for building with AppImage file",
                                           "generate");
              parser.addPositionalArgument("id", "package id", "id");

              parser.process(app);

              auto args = parser.positionalArguments();
              auto projectName = args.value(1);

              if (projectName.isEmpty()) {
                  parser.showHelp(-1);
              }

              auto err = builder.generate(projectName);

              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        { "create",
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

              auto err = builder.create(projectName);

              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        { "build",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
              auto pkgVersion =
                QCommandLineOption("pversion", "set package version", "package version");
              auto srcVersion =
                QCommandLineOption("sversion", "set source version", "source version");
              auto srcCommit = QCommandLineOption("commit", "set commit refs", "source commit");
              auto buildOffline = QCommandLineOption("offline", "only use local repo", "");
              parser.addOptions({ execVerbose, pkgVersion, srcVersion, srcCommit, buildOffline });

              parser.addPositionalArgument("build", "build project", "build");

              parser.process(app);

              if (parser.isSet(execVerbose)) {
                  auto exec = linglong::util::splitExec(parser.value(execVerbose));
                  linglong::builder::BuilderConfig::instance()->setExec(exec);
              }

              linglong::builder::BuilderConfig::instance()->setOffline(parser.isSet(buildOffline));

              // config linglong.yaml before build if necessary
              if (parser.isSet(pkgVersion) || parser.isSet(srcVersion) || parser.isSet(srcCommit)) {
                  auto projectConfigPath =
                    QStringList{ linglong::builder::BuilderConfig::instance()->getProjectRoot(),
                                 "linglong.yaml" }
                      .join("/");

                  if (!QFileInfo::exists(projectConfigPath)) {
                      printer.printMessage("ll-builder should running in project root");
                      return -1;
                  }

                  auto [project, err] =
                    linglong::util::fromYAML<QSharedPointer<linglong::builder::Project>>(
                      projectConfigPath);
                  if (err) {
                      printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
                      return -1;
                  }

                  auto node = YAML::LoadFile(projectConfigPath.toStdString());

                  node["package"]["version"] = parser.value(pkgVersion).isEmpty()
                    ? project->package->version.toStdString()
                    : parser.value(pkgVersion).toStdString();

                  if (project->package->kind != linglong::builder::PackageKindRuntime) {
                      node["source"]["version"] = parser.value(srcVersion).isEmpty()
                        ? project->source->version.toStdString()
                        : parser.value(srcVersion).toStdString();

                      node["source"]["commit"] = parser.value(srcCommit).isEmpty()
                        ? project->source->commit.toStdString()
                        : parser.value(srcCommit).toStdString();
                  }
                  // fixme: use qt file stream
                  std::ofstream fout(projectConfigPath.toStdString());
                  fout << node;
              }
              auto err = builder.build();
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        { "run",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
              parser.addOptions({ execVerbose });

              parser.addPositionalArgument("run", "run project", "build");

              parser.process(app);

              if (parser.isSet(execVerbose)) {
                  auto exec = linglong::util::splitExec(parser.value(execVerbose));
                  linglong::builder::BuilderConfig::instance()->setExec(exec);
              }

              auto err = builder.run();
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        // { "export",
        //   [&](QCommandLineParser &parser) -> int {
        //       parser.clearPositionalArguments();

        //       parser.addPositionalArgument("export", "export build result to uab bundle",
        //       "export"); parser.addPositionalArgument(
        //         "filename",
        //         "bundle file name , if filename is empty,export default format bundle",
        //         "[filename]");

        //       auto localParam = QCommandLineOption("local", "make bundle with local directory",
        //       "");

        //       parser.addOptions({ localParam });
        //       parser.process(app);

        //       auto outputFilepath = parser.positionalArguments().value(1);
        //       bool useLocalDir = false;

        //       if (parser.isSet(localParam)) {
        //           useLocalDir = true;
        //       }

        //       auto err = builder.exportBundle(outputFilepath, useLocalDir);
        //       if (err) {
        //           qCritical() << err;
        //       }
        //       return err.code();
        //   } },
        { "export",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("export", "export build result to layer", "export");
              parser.addPositionalArgument(
                "path",
                "layer file path, if the path is empty, export layer to the current directory",
                "[path]");

              parser.process(app);

              auto path = parser.positionalArguments().value(1);

              if (path.isEmpty()) {
                  path = linglong::builder::BuilderConfig::instance()->getProjectRoot();
              }

              auto err = builder.exportLayer(path);
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }
              return err.code();
          } },
        { "extract",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("extract",
                                           "extract the layer to a directory",
                                           "extract");
              parser.addPositionalArgument("layer", "layer file path", "[layer]");
              parser.addPositionalArgument("destination", "destination directory", "[destination]");

              parser.process(app);

              const auto layerPath = parser.positionalArguments().value(1);
              const auto destination = parser.positionalArguments().value(2);

              if (layerPath.isEmpty() || destination.isEmpty()) {
                  parser.showHelp(-1);
              }

              auto err = builder.extractLayer(layerPath, destination);
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }
              return err.code();
          } },
        { "config",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("config", "config user info", "config");
              auto optUserName = QCommandLineOption("name", "user name", "--name");
              auto optUserPassword = QCommandLineOption("password", "user password", "--password");
              parser.addOptions({ optUserName, optUserPassword });

              parser.process(app);
              auto userName = parser.value(optUserName);
              auto userPassword = parser.value(optUserPassword);
              auto err = builder.config(userName, userPassword);
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        // { "import",
        //   [&](QCommandLineParser &parser) -> int {
        //       parser.clearPositionalArguments();

        //       parser.addPositionalArgument("import", "import package data to local repo",
        //       "import");

        //       parser.process(app);

        //       auto err = builder.import();
        //       if (err) {
        //           qCritical() << err;
        //       }

        //       return err.code();
        //   } },
        { "import",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("import", "import layer to local repo", "import");

              parser.addPositionalArgument("path", "layer file path", "[path]");

              parser.process(app);

              auto path = parser.positionalArguments().value(1);

              if (path.isEmpty()) {
                  printer.printMessage("the layer path should be specified.");
                  parser.showHelp(-1);
              }

              auto err = builder.importLayer(path);
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }
              return err.code();
          } },
        { "track",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("track",
                                           "track the latest commit and update it",
                                           "track");

              parser.process(app);

              auto err = builder.track();
              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
        { "push",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              parser.addPositionalArgument("push", "push build result to repo", "push");

              auto optRepoUrl = QCommandLineOption("repo-url", "remote repo url", "--repo-url");
              auto optRepoName = QCommandLineOption("repo-name", "remote repo name", "--repo-name");
              auto optRepoChannel =
                QCommandLineOption("channel", "remote repo channel", "--channel", "main");
              auto optNoDevel = QCommandLineOption("no-devel", "push without devel", "");
              parser.addOptions({ optRepoUrl, optRepoName, optRepoChannel, optNoDevel });

              parser.process(app);

              auto repoUrl = parser.value(optRepoUrl);
              auto repoName = parser.value(optRepoName);
              auto repoChannel = parser.value(optRepoChannel);

              bool pushWithDevel = parser.isSet(optNoDevel) ? false : true;

              auto err = builder.push(repoUrl, repoName, repoChannel, pushWithDevel);

              if (err) {
                  printer.printErr(LINGLONG_ERR(err.code(), err.message()).value());
              }

              return err.code();
          } },
    };

    if (subcommandMap.contains(command)) {
        auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
