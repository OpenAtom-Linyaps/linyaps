/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/package/architecture.h"
#include "linglong/package/version.h"
#include "linglong/repo/client_factory.h"
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

#include <iostream>

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

QStringList projectBuildConfigPaths()
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

QStringList nonProjectBuildConfigPaths()
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

linglong::utils::error::Result<linglong::api::types::v1::BuilderProject>
parseProjectConfig(QString filename)
{
    LINGLONG_TRACE(QString("parse project config %1").arg(filename));
    auto project =
      linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(filename);
    if (!project) {
        return project;
    }
    auto version = linglong::package::Version(QString::fromStdString(project->package.version));
    if (!version.tweak) {
        return LINGLONG_ERR("Please ensure the package.version number has three parts formatted as "
                            "'MAJOR.MINOR.PATCH.TWEAK'");
    }
    return project;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // 初始化 qt qrc
    Q_INIT_RESOURCE(builder_releases);
    using namespace linglong::utils::global;
    // 初始化应用，builder在非tty环境也输出日志
    applicationInitializte(true);

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

    QStringList subCommandList = { "create", "build",  "run",     "export",
                                   "push",   "import", "extract", "repo" };

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
        if (projectDir.exists()) {
            qCritical() << projectName << "project dir already exists";
            return -1;
        }

        auto ret = projectDir.mkpath(".");
        if (!ret) {
            qCritical() << "create project dir failed";
            return -1;
        }

        auto configFilePath = projectDir.absoluteFilePath("linglong.yaml");
        auto templateFilePath = LINGLONG_DATA_DIR "/builder/templates/example.yaml";

        if (!QFileInfo::exists(templateFilePath)) {
            templateFilePath = ":/example.yaml";
        }
        QFile templateFile(templateFilePath);
        QFile configFile(configFilePath);
        if (!templateFile.open(QIODevice::ReadOnly)) {
            qDebug() << templateFilePath << templateFile.error();
            return -1;
        }

        if (!configFile.open(QIODevice::WriteOnly)) {
            qDebug() << configFilePath << configFile.error();
            return -1;
        }

        auto rawData = templateFile.readAll();
        rawData.replace("@ID@", projectName.toUtf8());
        if (!configFile.write(rawData)) {
            qDebug() << configFilePath << configFile.error();
            return -1;
        }

        return 0;
    }

    QStringList configPaths = {};
    // 初始化 build config
    initDefaultBuildConfig();
    configPaths << projectBuildConfigPaths();
    configPaths << nonProjectBuildConfigPaths();

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
    linglong::repo::ClientFactory clientFactory(repoCfg->repos[repoCfg->defaultRepo]);
    linglong::repo::OSTreeRepo repo(QString::fromStdString(builderCfg->repo),
                                    *repoCfg,
                                    clientFactory);

    auto containerBuidler = new linglong::runtime::ContainerBuilder(**ociRuntime);
    containerBuidler->setParent(QCoreApplication::instance());

    QMap<QString, std::function<int(QCommandLineParser & parser)>> subcommandMap = {
        { "repo",
          [&](QCommandLineParser &parser) -> int {
              parser.clearPositionalArguments();

              parser.addPositionalArgument("add", "add a remote repo");
              parser.addPositionalArgument("remove", "remove existing repo");
              parser.addPositionalArgument("update", "update url of existing repo");
              parser.addPositionalArgument("use", "set default repo");
              parser.addPositionalArgument("show", "show current config", "show\n");
              parser.setApplicationDescription("ll-builder repo add --name=NAME --url=URL\n"
                                               "ll-builder repo remove --name=NAME\n"
                                               "ll-builder repo update --name=NAME --url=NEWURL\n"
                                               "ll-builder repo use --name=NAME\n"
                                               "ll-builder repo show");

              auto name = QCommandLineOption("name", "name of remote repo", "name");
              auto url = QCommandLineOption("url", "url of remote repo", "url");
              parser.addOptions({ name, url });
              parser.process(app);

              QStringList args = parser.positionalArguments();
              if (args.size() < 2) {
                  std::cerr << "please specifying an operation" << std::endl;
                  return EINVAL;
              }

              linglong::utils::error::Result<void> ret;
              const auto &operation = args.at(1);
              if (operation == "add") {
                  if (!parser.isSet(name) || !parser.isSet(url)) {
                      std::cerr << "please specifying the repo name and url" << std::endl;
                      return EINVAL;
                  }
                  ret = repo.addRemoteRepo(parser.value(name), parser.value(url));
              } else if (operation == "remove") {
                  if (!parser.isSet(name)) {
                      std::cerr << "please specifying the repo name" << std::endl;
                      return EINVAL;
                  }
                  ret = repo.removeRemoteRepo(parser.value(name));
              } else if (operation == "update") {
                  if (!parser.isSet(name) || !parser.isSet(url)) {
                      std::cerr << "please specifying the repo name and url" << std::endl;
                      return EINVAL;
                  }
                  ret = repo.updateRemoteRepo(parser.value(name), parser.value(url));
              } else if (operation == "use") {
                  if (!parser.isSet(name)) {
                      std::cerr << "please specifying the repo name" << std::endl;
                      return EINVAL;
                  }
                  ret = repo.setDefaultRemoteRepo(parser.value(name));
              } else if (operation == "show") {
                  const auto &cfg = repo.getConfig();
                  auto &output = std::cout;
                  output << "version: " << cfg.version << "\ndefaultRepo: " << cfg.defaultRepo
                         << "\nrepos:\n"
                         << "name\turl\n";
                  std::for_each(cfg.repos.cbegin(), cfg.repos.cend(), [](const auto &pair) {
                      const auto &[name, url] = pair;
                      output << name << '\t' << url << "\n";
                  });
              } else {
                  std::cerr << "unknown operation:" << operation.toStdString();
                  return EINVAL;
              }

              if (!ret) {
                  std::cerr << ret.error().message().toStdString();
                  return -1;
              }

              return 0;
          } },
        { "build",
          [&](QCommandLineParser &parser) -> int {
              LINGLONG_TRACE("command build");

              parser.clearPositionalArguments();
              auto yamlFile =
                QCommandLineOption("f",
                                   "file path of the linglong.yaml (default is ./linglong.yaml)",
                                   "path",
                                   "linglong.yaml");
              auto execVerbose =
                QCommandLineOption("exec", "run exec than build script", "command");
              auto buildOffline = QCommandLineOption(
                "offline",
                "only use local files. This implies --skip-fetch-source and --skip-pull-depend",
                "");
              auto buildSkipFetchSource =
                QCommandLineOption("skip-fetch-source", "skip fetch sources", "");
              auto buildSkipPullDepend =
                QCommandLineOption("skip-pull-depend", "skip pull dependency", "");
              auto buildSkipRunContainer =
                QCommandLineOption("skip-run-container",
                                   "skip run container. This implies skip-commit-output",
                                   "");
              auto buildSkipCommitOutput =
                QCommandLineOption("skip-commit-output", "skip commit build output", "");
              auto buildArch = QCommandLineOption("arch", "set the build arch", "arch");

              parser.addOptions({ yamlFile,
                                  execVerbose,
                                  buildOffline,
                                  buildSkipFetchSource,
                                  buildSkipPullDepend,
                                  buildSkipRunContainer,
                                  buildSkipCommitOutput,
                                  buildArch });

              parser.addPositionalArgument("build", "build project", "build");
              parser.setApplicationDescription("linglong build command tools\n"
                                               "Examples:\n"
                                               "ll-builder build -v\n"
                                               "ll-builder build -v -- bash -c \"echo hello\"");

              parser.process(app);
              auto project = parseProjectConfig(QDir().absoluteFilePath(parser.value(yamlFile)));
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
                  auto arch =
                    linglong::package::Architecture::parse(parser.value(buildArch).toStdString());
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
              if (parser.isSet(buildSkipRunContainer)) {
                  auto cfg = builder.getConfig();
                  cfg.skipRunContainer = true;
                  cfg.skipCommitOutput = true;
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

              auto yamlFile =
                QCommandLineOption("f",
                                   "file path of the linglong.yaml (default is ./linglong.yaml)",
                                   "path",
                                   "linglong.yaml");
              auto execVerbose =
                QCommandLineOption("exec", "run exec than build script", "command");
              auto buildOffline = QCommandLineOption("offline", "only use local files.", "");
              parser.addOptions({ yamlFile, execVerbose, buildOffline });

              parser.addPositionalArgument("run", "run project", "build");

              parser.process(app);

              auto project = parseProjectConfig(QDir().absoluteFilePath(parser.value(yamlFile)));
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
              if (parser.isSet(buildOffline)) {
                  auto cfg = builder.getConfig();
                  cfg.skipFetchSource = true;
                  cfg.skipPullDepend = true;
                  cfg.offline = true;
                  builder.setConfig(cfg);
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

              auto yamlFile =
                QCommandLineOption({ "f", "file" },
                                   "file path of the linglong.yaml (default is ./linglong.yaml)",
                                   "path",
                                   "linglong.yaml");
              auto iconFile = QCommandLineOption({ "i", "icon" }, "uab icon (optional)", "path");
              auto layerMode = QCommandLineOption({ "l", "layer" }, "export layer file");
              parser.addOptions({ yamlFile, iconFile, layerMode });
              parser.process(app);

              auto project = parseProjectConfig(QDir().absoluteFilePath(parser.value(yamlFile)));
              if (!project) {
                  qCritical() << project.error();
                  return -1;
              }

              linglong::builder::Builder builder(*project,
                                                 QDir::current(),
                                                 repo,
                                                 *containerBuidler,
                                                 *builderCfg);

              if (parser.isSet(layerMode)) {
                  auto result = builder.exportLayer(QDir::currentPath());
                  if (!result) {
                      qCritical() << result.error();
                      return -1;
                  }

                  return 0;
              }

              auto result = builder.exportUAB(
                QDir::currentPath(),
                { .iconPath = parser.value(iconFile), .exportDevelop = true, .exportI18n = true });
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

              auto yamlFile =
                QCommandLineOption("f",
                                   "file path of the linglong.yaml (default is ./linglong.yaml)",
                                   "path",
                                   "linglong.yaml");
              parser.addOptions({
                yamlFile,
              });
              parser.addPositionalArgument("push", "push build result to repo", "push");

              auto optRepoUrl = QCommandLineOption("repo-url", "remote repo url", "--repo-url");
              auto optRepoName = QCommandLineOption("repo-name", "remote repo name", "--repo-name");
              auto optRepoChannel =
                QCommandLineOption("channel", "remote repo channel", "--channel", "main");
              auto optNoDevel = QCommandLineOption("no-develop", "push without develop", "");
              parser.addOptions({ yamlFile, optRepoUrl, optRepoName, optRepoChannel, optNoDevel });

              parser.process(app);

              auto repoUrl = parser.value(optRepoUrl);
              auto repoName = parser.value(optRepoName);
              auto repoChannel = parser.value(optRepoChannel);

              bool pushWithDevel = parser.isSet(optNoDevel) ? false : true;
              auto project = parseProjectConfig(QDir().absoluteFilePath(parser.value(yamlFile)));
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
