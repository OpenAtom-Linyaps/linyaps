/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/builder_config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/builder/project.h"
#include "linglong/cli/printer.h"
#include "linglong/package/architecture.h"
#include "linglong/service/app_manager.h"
#include "linglong/util/qserializer/yaml.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/systemd_sink.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMap>
#include <QRegExp>

#include <fstream>

#include <sys/wait.h>

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

    std::unique_ptr<spdlog::logger> logger;
    {
        auto sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>(
          { std::make_shared<spdlog::sinks::systemd_sink_mt>("ocppi") });
        if (isatty(stderr->_fileno)) {
            sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
        }

        logger = std::make_unique<spdlog::logger>("ocppi", sinks.begin(), sinks.end());

        logger->set_level(spdlog::level::trace);
    }
    auto path = QStandardPaths::findExecutable("crun");
    if (path.isEmpty()) {
        qCritical() << "crun not found";
        return -1;
    }
    auto crun = ocppi::cli::crun::Crun::New(path.toStdString(), logger);
    if (!crun.has_value()) {
        std::rethrow_exception(crun.error());
    }
    linglong::builder::BuilderConfig::instance()->setProjectRoot(QDir::currentPath());

    QCommandLineParser parser;

    auto optVerbose = QCommandLineOption({ "v", "verbose" }, "show detail log", "");
    parser.addOptions({ optVerbose });
    parser.addHelpOption();

    QStringList subCommandList = { "create", "build", "run", "export", "push", "convert" };

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
    linglong::service::AppManager appManager(ostree, *crun->get());
    linglong::builder::LinglongBuilder builder(ostree, printer, *crun->get(), appManager);

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        { "convert",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();
              auto pkgFile =
                QCommandLineOption({ "f", "file" },
                                   "app package file, it not required option, you can ignore"
                                   "this option when you set --url option and --hash option",
                                   "*.deb,*.AppImage(*.appimage)");
              auto pkgUrl = QCommandLineOption({ "u", "url" },
                                               "pkg url, it not required option, you can ignore"
                                               "this option when you set -f option",
                                               "pkg url");
              auto pkgHash = QCommandLineOption({ "hs", "hash" },
                                                "pkg hash value, it must be used with --url option",
                                                "pkg hash value");
              auto pkgID =
                QCommandLineOption({ "i", "id" }, "the unique name of the app", "app id");
              auto pkgName =
                QCommandLineOption({ "n", "name" }, "the description the app", "app description");
              auto pkgVersion =
                // -v is used for --verbose, so use -V replaced
                QCommandLineOption({ "V", "version" }, "the version of the app", "app version");
              auto pkgDescription = QCommandLineOption({ "d", "description" },
                                                       "detailed description of the app",
                                                       "app description");
              auto scriptOpt =
                QCommandLineOption({ "o", "output" },
                                   "not required option, it will "
                                   "generate linglong.yaml and script, you can modify "
                                   "linglong.yaml,then enter the directory(app name) and execute "
                                   "the script to generate the linglong .layer(.uab)",
                                   "script name");
              parser.addOptions({ pkgFile,
                                  pkgUrl,
                                  pkgHash,
                                  pkgID,
                                  pkgName,
                                  pkgVersion,
                                  pkgDescription,
                                  scriptOpt });
              parser.addPositionalArgument(
                "convert",
                "convert app with (deb,AppImage(appimage)) format to linglong format, you can "
                "generate convert config file by use -o option",
                "convert");

              parser.process(app);

              // file option or url option is required option
              if (!parser.isSet(pkgFile) && !parser.isSet(pkgUrl)) {
                  printer.printMessage("file option or url option is required");
                  parser.showHelp(-1);
                  return -1;
              }

              // hash option is required option when use url option
              if (parser.isSet(pkgUrl) && !parser.isSet(pkgHash)) {
                  printer.printMessage("hash option is required when use url option");
                  parser.showHelp(-1);
                  return -1;
              }

              QFileInfo fileInfo(parser.isSet(pkgFile) ? parser.value(pkgFile)
                                                       : parser.value(pkgUrl));
              auto fileSuffix = fileInfo.suffix();
              auto appImageFileType = fileSuffix == "AppImage" || fileSuffix == "appimage";
              auto debFileType = fileSuffix == "deb";

              if (!(appImageFileType || debFileType)) {
                  printer.printMessage(QString("unsupported %1 file type to convert, please "
                                               "specify (deb,AppImage(appimage)) file")
                                         .arg(fileSuffix));
                  parser.showHelp(-1);
                  return -1;
              }

              if (parser.isSet(pkgID) || parser.isSet(pkgName) || parser.isSet(pkgVersion)
                  || parser.isSet(pkgDescription) || parser.isSet(scriptOpt)) {
                  if (appImageFileType) {
                      auto ret = builder.appimageConvert(
                        QStringList()
                        << parser.value(pkgFile) << parser.value(pkgUrl) << parser.value(pkgHash)
                        << parser.value(pkgID) << parser.value(pkgName) << parser.value(pkgVersion)
                        << parser.value(pkgDescription) << parser.value(scriptOpt));
                      if (!ret) {
                          printer.printErr(ret.error());
                          return ret.error().code();
                      }
                  } else {
                      // TODO: implement deb convert

                      LINGLONG_TRACE("unsupported type");
                      auto err = LINGLONG_ERR(fileSuffix).value();
                      printer.printErr(err);
                      return err.code();
                  }
              }

              return 0;
          } },
        { "create",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command create");

              parser.clearPositionalArguments();
              parser.addPositionalArgument("create", "create build template project", "create");
              parser.addPositionalArgument("name", "project name", "<org.deepin.demo>");

              parser.process(app);

              auto args = parser.positionalArguments();
              auto projectName = args.value(1);

              if (projectName.isEmpty()) {
                  parser.showHelp(-1);
              }

              auto oldErr = builder.create(projectName);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }

              return 0;
          } },
        { "build",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command build");

              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
              auto pkgVersion =
                QCommandLineOption("pversion", "set package version", "package version");
              auto srcVersion =
                QCommandLineOption("sversion", "set source version", "source version");
              auto srcCommit = QCommandLineOption("commit", "set commit refs", "source commit");
              auto buildOffline = QCommandLineOption("offline", "only use local repo", "");
              auto buildArch = QCommandLineOption("arch", "set the build arch", "arch");
              auto buildEnv = QCommandLineOption("env", "set environment variables", "env");
              parser.addOptions({ execVerbose,
                                  pkgVersion,
                                  srcVersion,
                                  srcCommit,
                                  buildOffline,
                                  buildArch,
                                  buildEnv });

              parser.addPositionalArgument("build", "build project", "build");

              parser.process(app);

              auto buildConfig = linglong::builder::BuilderConfig::instance();
              if (parser.isSet(execVerbose)) {
                  auto exec = linglong::util::splitExec(parser.value(execVerbose));
                  buildConfig->setExec(exec);
              }

              buildConfig->setOffline(parser.isSet(buildOffline));

              if (parser.isSet(buildArch)) {
                  auto arch = linglong::package::Architecture::parse(parser.value(buildArch));
                  if (!arch.has_value()) {
                      printer.printErr(arch.error());
                      return -1;
                  }
                  buildConfig->setBuildArch((*arch).toString());
              }
              if (parser.isSet(buildEnv)) {
                  buildConfig->setBuildEnv(parser.values(buildEnv));
              }

              // config linglong.yaml before build if necessary
              if (parser.isSet(pkgVersion) || parser.isSet(srcVersion) || parser.isSet(srcCommit)) {
                  auto projectConfigPath =
                    QStringList{ buildConfig->getProjectRoot(), "linglong.yaml" }.join("/");

                  if (!QFileInfo::exists(projectConfigPath)) {
                      printer.printMessage("ll-builder should running in project root");
                      return -1;
                  }

                  auto [project, oldErr] =
                    linglong::util::fromYAML<QSharedPointer<linglong::builder::Project>>(
                      projectConfigPath);
                  if (oldErr) {
                      auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                      printer.printErr(err);
                      return err.code();
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

              auto child_pid = fork();
              if (child_pid == 0) {
                  auto ret = builder.build();
                  if (!ret.has_value()) {
                      printer.printErr(ret.error());
                      return ret.error().code();
                  }
                  return 0;
              }
              sleep(1);
              QString pidStr = QString("%1").arg(child_pid);
              auto ret = linglong::utils::command::Exec(
                "newuidmap",
                { pidStr, "0", "1000", "1", "1", "100000", "65536" });
              if (!ret.has_value()) {
                  printer.printErr(ret.error());
                  return ret.error().code();
              }
              ret = linglong::utils::command::Exec(
                "newgidmap",
                { pidStr, "0", "1000", "1", "1", "100000", "65536" });
              if (!ret.has_value()) {
                  printer.printErr(ret.error());
                  return ret.error().code();
              }
              return waitpid(child_pid, NULL, 0);
          } },
        { "run",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command run");

              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "exec");
              parser.addOptions({ execVerbose });

              parser.addPositionalArgument("run", "run project", "build");

              parser.process(app);

              if (parser.isSet(execVerbose)) {
                  auto exec = linglong::util::splitExec(parser.value(execVerbose));
                  linglong::builder::BuilderConfig::instance()->setExec(exec);
              }

              auto oldErr = builder.run();
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }

              return 0;
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
              LINGLONG_TRACE("command export");

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

              auto oldErr = builder.exportLayer(path);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }

              return 0;
          } },
        { "extract",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command extract");

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

              auto oldErr = builder.extractLayer(layerPath, destination);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }

              return 0;
          } },
        { "config",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command config");

              parser.clearPositionalArguments();

              parser.addPositionalArgument("config", "config user info", "config");
              auto optUserName = QCommandLineOption("name", "user name", "--name");
              auto optUserPassword = QCommandLineOption("password", "user password", "--password");
              parser.addOptions({ optUserName, optUserPassword });

              parser.process(app);
              auto userName = parser.value(optUserName);
              auto userPassword = parser.value(optUserPassword);
              auto oldErr = builder.config(userName, userPassword);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }

              return 0;
          } },
        { "import",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command import");

              parser.clearPositionalArguments();

              parser.addPositionalArgument("import", "import layer to local repo", "import");

              parser.addPositionalArgument("path", "layer file path", "[path]");

              parser.process(app);

              auto path = parser.positionalArguments().value(1);

              if (path.isEmpty()) {
                  printer.printMessage("the layer path should be specified.");
                  parser.showHelp(-1);
              }

              auto oldErr = builder.importLayer(path);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }
              return 0;
          } },
        { "track",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command track");
              parser.clearPositionalArguments();

              parser.addPositionalArgument("track",
                                           "track the latest commit and update it",
                                           "track");

              parser.process(app);

              auto oldErr = builder.track();
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }
              return 0;
          } },
        { "push",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command push");

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

              auto oldErr = builder.push(repoUrl, repoName, repoChannel, pushWithDevel);
              if (oldErr) {
                  auto err = LINGLONG_ERR(oldErr.message(), oldErr.code()).value();
                  printer.printErr(err);
                  return err.code();
              }
              return 0;
          } },
    };

    if (subcommandMap.contains(command)) {
        auto subcommand = subcommandMap[command];
        return subcommand(parser);
    } else {
        parser.showHelp();
    }
}
