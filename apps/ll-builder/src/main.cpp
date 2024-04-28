/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/package/architecture.h"
#include "linglong/repo/config.h"
#include "linglong/utils/command/env.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/serialize/yaml.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMap>
#include <QRegExp>

#include <fstream>

#include <wordexp.h>

namespace {

QStringList splitExec(const QString &exec)
{
    auto words = exec.toStdString();
    wordexp_t p;
    auto ret = wordexp(words.c_str(), &p, WRDE_SHOWERR);
    if (ret != 0) {
        QString errMessage;
        switch (ret) {
        case WRDE_BADCHAR:
            errMessage = "BADCHAR";
            qWarning() << "wordexp error: " << errMessage;
            return {};
        case WRDE_BADVAL:
            errMessage = "BADVAL";
            break;
        case WRDE_CMDSUB:
            errMessage = "CMDSUB";
            break;
        case WRDE_NOSPACE:
            errMessage = "NOSPACE";
            break;
        case WRDE_SYNTAX:
            errMessage = "SYNTAX";
            break;
        default:
            errMessage = "unknown";
        }
        qWarning() << "wordexp error: " << errMessage;
        wordfree(&p);
        return {};
    }
    QStringList res;
    for (int i = 0; i < (int)p.we_wordc; i++) {
        res << p.we_wordv[i];
    }
    wordfree(&p);
    return res;
}

QStringList projectConfigPaths()
{
    QStringList result{};

    auto pwd = QDir::current();

    do {
        auto configPath =
          QStringList{ pwd.absolutePath(), ".ll-builder", "config.yaml" }.join(QDir::separator());
        result << std::move(configPath);
    } while (pwd.cdUp());

    return result;
}

QStringList nonProjectConfigPaths()
{
    QStringList result{};

    auto configLocations = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    configLocations.append(SYSCONFDIR);

    for (const auto &configLocation : configLocations) {
        result << QStringList{ configLocation, "linglong", "builder", "config.yaml" }.join(
          QDir::separator());
    }

    result << QStringList{ DATADIR, "linglong", "builder", "config.yaml" }.join(QDir::separator());

    return result;
}

void initDefaultBuildConfig()
{
    // ~/.cache
    QDir cacheLocation = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    // ~/.config/
    QDir configLocations = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (!QDir().mkpath(configLocations.filePath("linglong/builder"))) {
        qWarning() << "init BuildConfig directory failed."
                   << configLocations.filePath("linglong/builder");
    }
    QString configFilePath = configLocations.filePath("linglong/builder/config.yaml");
    if (QFile::exists(configFilePath)) {
        return;
    }
    linglong::api::types::v1::BuilderConfig config;
    config.version = 1;
    config.repo = cacheLocation.filePath("linglong-builder").toStdString();
    linglong::builder::saveConfig(config, configFilePath);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // 初始化 qt qrc
    Q_INIT_RESOURCE(builder_releases);
    using namespace linglong::utils::global;

    applicationInitializte();

    auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
    if (ociRuntimeCLI.isEmpty()) {
        ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
    }

    auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
    if (path.isEmpty()) {
        qCritical() << ociRuntimeCLI << "not found";
        return -1;
    }

    auto ociRuntime = ocppi::cli::crun::Crun::New(path.toStdString());
    if (!ociRuntime.has_value()) {
        std::rethrow_exception(ociRuntime.error());
    }

    QCommandLineParser parser;

    auto optVerbose = QCommandLineOption({ "v", "verbose" },
                                         "show detail log (deprecated, use QT_LOGGING_RULES)",
                                         "");
    parser.addOptions({ optVerbose });
    parser.addHelpOption();

    QStringList subCommandList = { "create", "build", "run", "export", "push", "convert", "import", "extract" };

    parser.addPositionalArgument("subcommand",
                                 subCommandList.join("\n"),
                                 "subcommand [sub-option]");

    parser.parse(QCoreApplication::arguments());

    QStringList args = parser.positionalArguments();
    QString command = args.isEmpty() ? QString() : args.first();
    if (command == "create") {
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

        QDir projectDir = QDir::current().absoluteFilePath(projectName);
        projectDir.mkpath(".");
        auto configFilePath = projectDir.absoluteFilePath("linglong.yaml");
        auto templateFilePath = LINGLONG_DATA_DIR "/buidler/templates/example.yaml";

        if (QFileInfo::exists(templateFilePath)) {
            QFile::copy(templateFilePath, configFilePath);
        } else {
            QFile::copy(":/example.yaml", configFilePath);
        }

        return 0;
    }
    auto scriptOpt =
      QCommandLineOption({ "o", "output" },
                         "not required option, it will "
                         "generate linglong.yaml and script, you can modify "
                         "linglong.yaml,then enter the directory(app name) and execute "
                         "the script to generate the linglong .layer(.uab)",
                         "script name");
    if (command == "convert") {
        parser.clearPositionalArguments();
        auto pkgFile = QCommandLineOption({ "f", "file" },
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
        auto pkgID = QCommandLineOption({ "i", "id" }, "the unique name of the app", "app id");
        auto pkgName =
          QCommandLineOption({ "n", "name" }, "the description the app", "app description");
        auto pkgVersion =
          // -v is used for --verbose, so use -V replaced
          QCommandLineOption({ "V", "version" }, "the version of the app", "app version");
        auto pkgDescription = QCommandLineOption({ "d", "description" },
                                                 "detailed description of the app",
                                                 "app description");
        parser.addOptions(
          { pkgFile, pkgUrl, pkgHash, pkgID, pkgName, pkgVersion, pkgDescription, scriptOpt });
        parser.addPositionalArgument(
          "convert",
          "convert app with (deb,AppImage(appimage)) format to linglong format, you can "
          "generate convert config file by use -o option",
          "convert");

        parser.process(app);

        // file option or url option is required option
        if (!parser.isSet(pkgFile) && !parser.isSet(pkgUrl)) {
            qCritical() << "file option or url option is required";
            parser.showHelp(-1);
            return -1;
        }

        // hash option is required option when use url option
        if (parser.isSet(pkgUrl) && !parser.isSet(pkgHash)) {
            qCritical() << "hash option is required when use url option";
            parser.showHelp(-1);
            return -1;
        }

        QFileInfo fileInfo(parser.isSet(pkgFile) ? parser.value(pkgFile) : parser.value(pkgUrl));
        auto fileSuffix = fileInfo.suffix();
        auto appImageFileType = fileSuffix == "AppImage" || fileSuffix == "appimage";

        if (!appImageFileType) {
            qCritical() << "unsupported file type" << fileSuffix;
            parser.showHelp(-1);
            return -1;
        }

        auto templateArgs = QStringList()
          << parser.value(pkgFile) << parser.value(pkgUrl) << parser.value(pkgHash)
          << parser.value(pkgID) << parser.value(pkgName) << parser.value(pkgVersion)
          << parser.value(pkgDescription);

        auto createPorject = [&]() -> linglong::utils::error::Result<void> {
            LINGLONG_TRACE("create appimage project");

            const auto file = templateArgs.at(0);
            const auto url = templateArgs.at(1);
            const auto hash = templateArgs.at(2);
            const auto id = templateArgs.at(3);
            const auto name = templateArgs.at(4);
            const auto version = templateArgs.at(5);
            const auto description = templateArgs.at(6);

            auto projectPath = QDir(name);
            if (!projectPath.mkpath(".")) {
                return LINGLONG_ERR("create " + projectPath.absolutePath() + ": failed");
            }

            auto projectFilePath = projectPath.absoluteFilePath("linglong.yaml");
            QString templateName = "appimage-local.yaml";
            if (file.isEmpty()) {
                templateName = "appimage-url.yaml";
            }
            auto templateFilePath = LINGLONG_DATA_DIR "builder/" + templateName;
            if (QFileInfo::exists(templateFilePath)) {
                QFile::copy(templateFilePath, projectFilePath);
            } else {
                QFile::copy(":/" + templateName, projectFilePath);
            }

            // copy appimage file to project path
            if (!file.isEmpty()) {
                QFileInfo fileInfo(file);
                const auto &sourcefilePath = fileInfo.absoluteFilePath();
                const auto &destinationFilePath = QDir(projectPath).filePath(fileInfo.fileName());

                if (!QFileInfo::exists(sourcefilePath)) {
                    return LINGLONG_ERR(QString("appimage file %1 not found").arg(sourcefilePath));
                }

                QFile::copy(sourcefilePath, destinationFilePath);
            }

            auto projectFile = QFile(projectFilePath);
            if (!projectFile.open(QFile::ReadOnly)) {
                return LINGLONG_ERR("open " + projectFilePath);
            }
            auto contents = projectFile.readAll();
            if (projectFile.error() != QFile::NoError) {
                return LINGLONG_ERR(projectFile);
            }
            projectFile.close();
            contents.replace("{{{ID}}}", id.toUtf8());
            contents.replace("{{{NAME}}}", name.toUtf8());
            contents.replace("{{{VERSION}}}", version.toUtf8());
            contents.replace("{{{DESCRIPTION}}}", description.toUtf8());

            if (file.isEmpty()) {
                contents.replace("{{{URL}}}", url.toUtf8());
                contents.replace("{{{DIGEST}}}", hash.toUtf8());
            }

            if (!projectFile.open(QFile::WriteOnly)) {
                return LINGLONG_ERR("open " + projectFilePath);
            }

            projectFile.write(contents);
            if (projectFile.error() != QFile::NoError) {
                return LINGLONG_ERR(projectFile);
            }

            if (!QDir::setCurrent(name)) {
                return LINGLONG_ERR("cd to " + name + ": failed");
            }

            return LINGLONG_OK;
        };

        auto result = createPorject();
        if (!result) {
            qCritical() << result.error();
            return -1;
        }
    }

    QStringList configPaths = {};
    // 初始化 build config
    initDefaultBuildConfig();
    configPaths << projectConfigPaths();
    configPaths << nonProjectConfigPaths();

    auto builderCfg = linglong::builder::loadConfig(configPaths);
    if (!builderCfg) {
        qCritical() << builderCfg.error();
        return -1;
    }

    auto repoCfg =
      linglong::repo::loadConfig({ QString::fromStdString(builderCfg->repo + "/config.yaml"),
                                   LINGLONG_DATA_DIR "/config.yaml" });
    if (!repoCfg) {
        qCritical() << repoCfg.error();
        return -1;
    }

    QNetworkAccessManager networkAccessManager;

    linglong::api::client::ClientApi api;
    api.setTimeOut(10 * 60 * 1000);
    api.setNetworkAccessManager(&networkAccessManager);
    api.setNewServerForAllOperations(QString::fromStdString(repoCfg->repos[repoCfg->defaultRepo]));

    linglong::repo::OSTreeRepo repo(QString::fromStdString(builderCfg->repo), *repoCfg, api);

    auto containerBuidler = new linglong::runtime::ContainerBuilder(**ociRuntime);
    containerBuidler->setParent(QCoreApplication::instance());

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        { "convert",
          [&](QCommandLineParser &parser) -> int {
              const auto scriptName = parser.value(scriptOpt);

              // if user not specified -o option, export linglong .layer(.uab) directly, or
              // generate linglong.yaml and convert.sh
              if (!scriptName.isEmpty()) {
                  return 0;
              }

              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              auto result = builder.build({ "/source/linglong/entry.sh" });
              if (!result) {
                  qCritical() << result.error();
                  return -1;
              }

              result = builder.exportLayer(QDir::current().absolutePath());
              if (!result) {
                  qCritical() << result.error();
                  return -1;
              }

              // delete the generated temporary file, only keep .layer(.uab) files
              auto output = linglong::utils::command::Exec(
                "bash",
                QStringList() << "-c"
                              << "find . -maxdepth 1 -not -regex '.*\\.\\|.*\\.layer\\|.*\\.uab' "
                                 "-exec basename {} -print0 \\;  | xargs rm -r");
              if (!output) {
                  qCritical() << output.error();
                  return -1;
              }

              return 0;
          } },
        { "build",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command build");

              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "command");
              auto buildOffline = QCommandLineOption(
                "offline",
                "only use local files. This implies --skip-fetch-source and --skip-pull-depend",
                "");
              auto buildSkipFetchSource =
                QCommandLineOption("skip-fetch-source", "skip fetch sources", "");
              auto buildSkipPullDepend =
                QCommandLineOption("skip-pull-depend", "skip pull dependency", "");
              auto buildSkipCommitOutput =
                QCommandLineOption("skip-commit-output", "skip commit build output", "");
              auto buildArch = QCommandLineOption("arch", "set the build arch", "arch");

              parser.addOptions({ execVerbose,
                                  buildOffline,
                                  buildSkipFetchSource,
                                  buildSkipPullDepend,
                                  buildSkipCommitOutput,
                                  buildArch });

              parser.addPositionalArgument("build", "build project", "build");
              parser.setApplicationDescription("linglong build command tools\n"
                                               "Examples:\n"
                                               "ll-builder build -v\n"
                                               "ll-builder build -v -- bash -c \"echo hello\"");

              parser.process(app);
              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);

              if (parser.isSet(buildArch)) {
                  auto arch = linglong::package::Architecture::parse(parser.value(buildArch));
                  if (!arch) {
                      qCritical() << arch.error();
                      return -1;
                  }
                  auto cfg = builder.getConfig();
                  cfg.arch = arch->toString().toStdString();
                  builder.setConfig(cfg);
              }
              if (parser.isSet(buildSkipFetchSource)) {
                  auto cfg = builder.getConfig();
                  cfg.skipFetchSource = true;
                  builder.setConfig(cfg);
              }
              if (parser.isSet(buildSkipPullDepend)) {
                  auto cfg = builder.getConfig();
                  cfg.skipPullDepend = true;
                  builder.setConfig(cfg);
              }
              if (parser.isSet(buildSkipCommitOutput)) {
                  auto cfg = builder.getConfig();
                  cfg.skipCommitOutput = true;
                  builder.setConfig(cfg);
              }
              if (parser.isSet(buildOffline)) {
                  auto cfg = builder.getConfig();
                  cfg.skipFetchSource = true;
                  cfg.skipPullDepend = true;
                  cfg.offline = true;
                  builder.setConfig(cfg);
              }
              auto allArgs = QCoreApplication::arguments();
              linglong::utils::error::Result<void> ret;
              if (parser.isSet(execVerbose)) {
                  auto exec = splitExec(parser.value(execVerbose));
                  ret = builder.build(exec);
              } else if (allArgs.indexOf("--") > 0) {
                  auto exec = allArgs.mid(allArgs.indexOf("--") + 1);
                  ret = builder.build(exec);
              } else {
                  ret = builder.build();
              }
              if (!ret) {
                  qCritical() << ret.error();
                  return ret.error().code();
              }
              return 0;
          } },
        { "run",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command run");

              parser.clearPositionalArguments();

              auto execVerbose = QCommandLineOption("exec", "run exec than build script", "command");
              parser.addOptions({ execVerbose });

              parser.addPositionalArgument("run", "run project", "build");

              parser.process(app);

              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              QStringList exec;
              if (parser.isSet(execVerbose)) {
                  exec = splitExec(parser.value(execVerbose));
              }

              auto result = builder.run(exec);
              if (!result) {
                  qCritical() << result.error();
                  return -1;
              }

              return 0;
          } },
        { "export",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command export");
              parser.clearPositionalArguments();
              parser.process(app);

              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              auto result = builder.exportLayer(QDir().absolutePath());
              if (!result) {
                  qCritical() << result.error();
                  return -1;
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

              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              auto result = builder.extractLayer(layerPath, destination);
              if (!result) {
                  qCritical() << result.error();
                  return -1;
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
                  qCritical() << "the layer path should be specified.";
                  parser.showHelp(-1);
              }

              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              auto result = builder.importLayer(path);
              if (!result) {
                  qCritical() << result.error();
                  return -1;
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
              auto optNoDevel = QCommandLineOption("no-develop", "push without develop", "");
              parser.addOptions({ optRepoUrl, optRepoName, optRepoChannel, optNoDevel });

              parser.process(app);

              auto repoUrl = parser.value(optRepoUrl);
              auto repoName = parser.value(optRepoName);
              auto repoChannel = parser.value(optRepoChannel);

              bool pushWithDevel = parser.isSet(optNoDevel) ? false : true;
              auto project =
                linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(
                  QDir().absoluteFilePath("linglong.yaml"));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);
              auto result = builder.push(pushWithDevel, repoUrl, repoName);
              if (!result) {
                  qCritical() << result.error();
                  return -1;
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
