/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/builder.h"
#include "linglong/builder/builder_config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/builder/project.h"
#include "linglong/package/package.h"
#include "linglong/repo/repo.h"
#include "linglong/util/connection.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/global/initialize.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QMap>
#include <QRegExp>

#include <fstream>

int main(int argc, char **argv)
{
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

    if (parser.isSet(optVerbose)) {
        qputenv("QT_LOGGING_RULES", "*=true");
    }

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();

    QNetworkAccessManager networkAccessManager;

    linglong::api::client::ClientApi api;
    api.setTimeOut(5000);
    api.setNetworkAccessManager(&networkAccessManager);
    api.setNewServerForAllOperations(
      linglong::builder::BuilderConfig::instance()->remoteRepoEndpoint);

    auto config = linglong::repo::config::ConfigV1{
        linglong::builder::BuilderConfig::instance()->remoteRepoName.toStdString(),
        { { linglong::builder::BuilderConfig::instance()->remoteRepoName.toStdString(),
            linglong::builder::BuilderConfig::instance()->remoteRepoEndpoint.toStdString() } },
        1
    };
    auto repoPath = linglong::builder::BuilderConfig::instance()->repoPath();
    linglong::util::Connection dbConnection(QDir(repoPath).filePath("repo.sqlite"));
    linglong::repo::OSTreeRepo ostree(repoPath, config, api, dbConnection);

    linglong::builder::LinglongBuilder builder(ostree);

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
                  qDebug() << err;
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
                  qDebug() << err;
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
                      qCritical() << "ll-builder should running in project root";
                      return -1;
                  }

                  auto [project, err] =
                    linglong::util::fromYAML<QSharedPointer<linglong::builder::Project>>(
                      projectConfigPath);
                  if (err) {
                      qCritical() << "load linglong yaml" << err.code() << err.message();
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
                  qCritical() << err;
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
                  qCritical() << err;
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
                  qCritical() << err;
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
                  qCritical() << err;
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
                  qCritical() << "the layer path should be specified.";
                  parser.showHelp(-1);
              }

              auto err = builder.importLayer(path);
              if (err) {
                  qCritical() << err;
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
                  qCritical() << err;
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
                  qCritical() << err;
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
