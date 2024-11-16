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
#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/global/initialize.h"
#include "linglong/utils/serialize/yaml.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <CLI/CLI.hpp>

#include <QCoreApplication>
#include <QStringList>

#include <iostream>
#include <list>
#include <ostream>
#include <string>
#include <vector>

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
        result << configPath;
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
parseProjectConfig(const QString &filename)
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
    if (project->modules.has_value()) {
        if (std::any_of(project->modules->begin(), project->modules->end(), [](const auto &module) {
                return module.name == "binary";
            })) {
            return LINGLONG_ERR("configuration of binary modules is not allowed. see "
                                "https://linglong.space/guide/ll-builder/modules.html");
        }
    }
    if (project->package.kind == "app" && !project->command.has_value()) {
        return LINGLONG_ERR(
          "'command' field is missing, app should hava command as the default startup command");
    }
    return project;
}

} // namespace

int main(int argc, char **argv)
{
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);
    QCoreApplication app(argc, argv);
    // 初始化 qt qrc
    Q_INIT_RESOURCE(builder_releases);
    using namespace linglong::utils::global;
    // 初始化应用，builder在非tty环境也输出日志
    applicationInitialize(true);

    CLI::App commandParser{ _("linyaps builder CLI \n"
                              "A CLI program to build linyaps application\n") };
    commandParser.get_help_ptr()->description(_("Print this help message and exit"));
    commandParser.set_help_all_flag("--help-all", _("Expand all help"));

    commandParser.usage(_("Usage: ll-builder [OPTIONS] [SUBCOMMAND]"));
    commandParser.footer([]() {
        return _(R"(If you found any problems during use
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues)");
    });

    CLI::Validator validatorString{
        [](const std::string &parameter) {
            if (parameter.empty()) {
                return std::string{ _(
                  "Input parameter is empty, please input valid parameter instead") };
            }
            return std::string();
        },
        ""
    };

    // add builder flags
    bool versionFlag = false;
    commandParser.add_flag("--version", versionFlag, _("Show version"));

    // add builder create
    std::string projectName;
    auto buildCreate =
      commandParser.add_subcommand("create", _("Create linyaps build template project"));
    buildCreate->usage(_("Usage: ll-builder create [OPTIONS] NAME"));
    buildCreate->add_option("NAME", projectName, _("Project name"))
      ->required()
      ->check(validatorString);

    // add builder build
    bool buildOffline = false, buildFullDevelopModule = false, buildSkipFetchSource = false,
         buildSkipPullDepend = false, buildSkipCheckOutput = false, buildSkipStripSymbols = false,
         buildSkipCommitOutput = false, buildSkipRunContainer = false;
    std::string filePath{ "./linglong.yaml" }, arch;
    // group empty will hide command
    std::string hiddenGroup = "";
    std::vector<std::string> oldCommands;
    std::vector<std::string> newCommands;
    auto buildBuilder = commandParser.add_subcommand("build", _("Build a linyaps project"));
    buildBuilder->usage(_("Usage: ll-builder build [OPTIONS] [COMMAND...]"));
    buildBuilder->add_option("--file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->capture_default_str()
      ->check(CLI::ExistingFile);
    buildBuilder->add_option("--arch", arch, _("Set the build arch"))
      ->type_name("ARCH")
      ->check(validatorString);
    buildBuilder->add_option(
      "COMMAND",
      newCommands,
      _("Enter the container to execute command instead of building applications"));
    buildBuilder
      ->add_option("--exec",
                   oldCommands,
                   _("Enter the container to execute command instead of building applications"))
      ->group(hiddenGroup);
    buildBuilder->add_flag("--offline",
                           buildOffline,
                           _("Only use local files. This implies --skip-fetch-source and "
                             "--skip-pull-depend will be set"));
    buildBuilder
      ->add_flag("--full-develop-module",
                 buildFullDevelopModule,
                 _("Build full develop packages, runtime requires"))
      ->group(hiddenGroup);
    buildBuilder->add_flag("--skip-fetch-source", buildSkipFetchSource, _("Skip fetch sources"));
    buildBuilder->add_flag("--skip-pull-depend", buildSkipPullDepend, _("Skip pull dependency"));
    buildBuilder->add_flag("--skip-run-container", buildSkipRunContainer, _("Skip run container"));
    buildBuilder->add_flag("--skip-commit-output",
                           buildSkipCommitOutput,
                           _("Skip commit build output"));
    buildBuilder->add_flag("--skip-output-check", buildSkipCheckOutput, _("Skip output check"));
    buildBuilder->add_flag("--skip-strip-symbols",
                           buildSkipStripSymbols,
                           _("Skip strip debug symbols"));

    // add builder run
    bool debugMode = false;
    std::vector<std::string> execModules;
    auto buildRun = commandParser.add_subcommand("run", _("Run builded linyaps app"));
    buildRun->usage(_("Usage: ll-builder run [OPTIONS] [COMMAND...]"));
    buildRun->add_option("--file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->capture_default_str()
      ->check(CLI::ExistingFile);
    buildRun->add_flag("--offline", buildOffline, _("Only use local files"));
    buildRun
      ->add_option("--modules",
                   execModules,
                   _("Run specified module. eg: --modules binary,develop"))
      ->delimiter(',')
      ->type_name("modules");
    buildRun->add_option(
      "COMMAND",
      newCommands,
      _("Enter the container to execute command instead of running application"));
    buildRun
      ->add_option("--exec",
                   oldCommands,
                   _("Enter the container to execute command instead of running application"))
      ->group(hiddenGroup);
    buildRun->add_flag("--debug", debugMode, _("Run in debug mode (enable develop module)"));

    // build export
    bool layerMode = false;
    std::string iconFile;
    auto buildExport = commandParser.add_subcommand("export", _("Export to linyaps layer or uab"));
    buildExport->usage(_("Usage: ll-builder export [OPTIONS]"));
    buildExport->add_option("--file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->capture_default_str()
      ->check(CLI::ExistingFile);
    buildExport->add_option("--icon", iconFile, _("Uab icon (optional)"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    buildExport->add_flag("--layer", layerMode, _("Export to linyaps layer file"));

    // build push
    std::string repoName, repoUrl, pushModule;
    auto buildPush = commandParser.add_subcommand("push", _("Push linyaps app to remote repo"));
    buildPush->usage(_("Usage: ll-builder push [OPTIONS]"));
    buildPush->add_option("--file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->capture_default_str()
      ->check(CLI::ExistingFile);
    buildPush->add_option("--repo-url", repoUrl, _("Remote repo url"))
      ->type_name("URL")
      ->check(validatorString);
    buildPush->add_option("--repo-name", repoUrl, _("Remote repo name"))
      ->type_name("NAME")
      ->check(validatorString);
    buildPush->add_option("--module", pushModule, _("Push single module"))->check(validatorString);

    // add build import
    std::string layerFile;
    auto buildImport =
      commandParser.add_subcommand("import", _("Import linyaps layer to build repo"));
    buildImport->usage(_("Usage: ll-builder import [OPTIONS] LAYER"));
    buildImport->add_option("LAYER", layerFile, _("Layer file path"))
      ->type_name("FILE")
      ->required()
      ->check(CLI::ExistingFile);

    // add build extract
    std::string dir;
    auto buildExtract = commandParser.add_subcommand("extract", _("Extract linyaps layer to dir"));
    buildExtract->usage(_("Usage: ll-builder extract [OPTIONS] LAYER DIR"));
    buildExtract->add_option("LAYER", layerFile, _("Layer file path"))
      ->required()
      ->check(CLI::ExistingFile);
    buildExtract->add_option("DIR", dir, _("Destination directory"))->type_name("DIR")->required();

    // add build repo
    auto buildRepo = commandParser.add_subcommand("repo", _("Display and manage repositories"));
    buildRepo->usage(_("Usage: ll-builder repo [OPTIONS] SUBCOMMAND"));
    buildRepo->require_subcommand(1);

    // add repo sub command add
    auto buildRepoAdd = buildRepo->add_subcommand("add", _("Add a new repository"));
    buildRepoAdd->usage(_("Usage: ll-builder repo add [OPTIONS] NAME URL"));
    buildRepoAdd->add_option("NAME", repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    buildRepoAdd->add_option("URL", repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command remove
    auto buildRepoRemove = buildRepo->add_subcommand("remove", _("Remove a repository"));
    buildRepoRemove->usage(_("Usage: ll-builder repo remove [OPTIONS] NAME"));
    buildRepoRemove->add_option("NAME", repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto buildRepoUpdate = buildRepo->add_subcommand("update", _("Update the repository URL"));
    buildRepoUpdate->usage(_("Usage: ll-builder repo update [OPTIONS] NAME URL"));
    buildRepoUpdate->add_option("NAME", repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    buildRepoUpdate->add_option("URL", repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto buildRepoSetDefault =
      buildRepo->add_subcommand("set-default", _("Set a default repository name"));
    buildRepoSetDefault->usage(_("Usage: ll-builder repo set-default [OPTIONS] NAME"));
    buildRepoSetDefault->add_option("NAME", repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command show
    auto buildRepoShow = buildRepo->add_subcommand("show", _("Show repository information"));
    buildRepoShow->usage(_("Usage: ll-builder repo show [OPTIONS]"));

    // add build migrate
    auto buildMigrate =
      commandParser.add_subcommand("migrate", _("Migrate repository data"))->group(hiddenGroup);

    CLI11_PARSE(commandParser, argc, argv);

    if (versionFlag) {
        std::cout << _("linyaps build tool version ") << LINGLONG_VERSION << std::endl;
        return 0;
    }

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

    if (buildCreate->parsed()) {
        auto name = QString::fromStdString(projectName);
        QDir projectDir = QDir::current().absoluteFilePath(name);
        if (projectDir.exists()) {
            qCritical() << name << "project dir already exists";
            return -1;
        }

        auto ret = projectDir.mkpath(".");
        if (!ret) {
            qCritical() << "create project dir failed";
            return -1;
        }

        auto configFilePath = projectDir.absoluteFilePath("linglong.yaml");
        const auto *templateFilePath = LINGLONG_DATA_DIR "/builder/templates/example.yaml";

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
        rawData.replace("@ID@", name.toUtf8());
        if (configFile.write(rawData) == 0) {
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

    auto repoRoot = QDir{ QString::fromStdString(builderCfg->repo) };
    if (!repoRoot.exists() && !repoRoot.mkpath(".")) {
        qCritical() << "failed to create the repository of builder.";
        return -1;
    }

    linglong::repo::OSTreeRepo repo(repoRoot, *repoCfg, clientFactory);

    // if user use old opt(--exec) passing parameters, pass it to the new commands;
    if (newCommands.empty() && !oldCommands.empty()) {
        newCommands = oldCommands;
    }

    QStringList commandList;
    if (!newCommands.empty() && (buildBuilder->parsed() || buildRun->parsed())) {
        for (const std::string &command : newCommands) {
            commandList.append(QString::fromStdString(command));
        }
    }

    if (buildMigrate->parsed()) {
        auto ret = repo.dispatchMigration();
        if (!ret) {
            qCritical() << "The underlying data may be corrupted, migration failed:"
                        << ret.error().message();
            return -1;
        }

        return 0;
    }

    if (repo.needMigrate()) {
        qFatal("underlying data needs migrating, please run 'll-builder migrate'");
    }

    auto *containerBuilder = new linglong::runtime::ContainerBuilder(**ociRuntime);
    containerBuilder->setParent(QCoreApplication::instance());
    linglong::builder::BuilderBuildOptions options;

    if (buildBuilder->parsed()) {
        auto yamlFile = QString::fromStdString(filePath);
        auto project = parseProjectConfig(QDir().absoluteFilePath(yamlFile));
        if (!project) {
            qCritical() << project.error();
            return -1;
        }

        linglong::builder::Builder builder(*project,
                                           QDir::current(),
                                           repo,
                                           *containerBuilder,
                                           *builderCfg);
        builder.projectYamlFile = QDir().absoluteFilePath(yamlFile).toStdString();
        if (!arch.empty()) {
            auto buildArch = QString::fromStdString(arch);
            auto arch = linglong::package::Architecture::parse(buildArch.toStdString());
            if (!arch) {
                qCritical() << arch.error();
                return -1;
            }
            auto cfg = builder.getConfig();
            cfg.arch = arch->toString().toStdString();
            builder.setConfig(cfg);
        }
        options.skipFetchSource = buildSkipFetchSource;
        options.skipPullDepend = buildSkipPullDepend;
        options.skipCommitOutput = buildSkipCommitOutput;
        options.skipCheckOutput = buildSkipCheckOutput;
        options.skipStripSymbols = buildSkipStripSymbols;
        options.fullDevelop = buildFullDevelopModule;
        if (buildSkipRunContainer) {
            options.skipCommitOutput = true;
            options.skipRunContainer = true;
        }
        if (buildOffline) {
            auto cfg = builder.getConfig();
            options.skipFetchSource = true;
            options.skipPullDepend = true;
            cfg.offline = true;
            builder.setConfig(cfg);
        }

        builder.setBuildOptions(options);

        linglong::utils::error::Result<void> ret;
        if (!newCommands.empty()) {
            auto execVerbose = commandList.join(" ");
            auto exec = splitExec(execVerbose);
            ret = builder.build(exec);
        } else {
            ret = builder.build();
        }
        if (!ret) {
            qCritical() << ret.error();
            return ret.error().code();
        }
        return 0;
    }

    if (buildRepo->parsed()) {
        if (buildRepoShow->parsed()) {
            const auto &cfg = repo.getConfig();
            // Note: keep the same format as ll-cli repo
            std::cout << "Default: " << cfg.defaultRepo << std::endl;
            std::cout << std::left << std::setw(11) << "Name";
            std::cout << "Url" << std::endl;
            for (const auto &r : cfg.repos) {
                std::cout << std::left << std::setw(10) << r.first << " " << r.second << std::endl;
            }
            return 0;
        }

        auto newCfg = repo.getConfig();

        if (!repoUrl.empty()) {
            if (repoUrl.rfind("http", 0) != 0) {
                std::cerr << "url is invalid." << std::endl;
                return EINVAL;
            }

            if (repoUrl.back() == '/') {
                repoUrl.pop_back();
            }
        }

        if (buildRepoAdd->parsed()) {
            if (repoUrl.empty()) {
                std::cerr << "url is empty." << std::endl;
                return EINVAL;
            }

            auto node = newCfg.repos.try_emplace(repoName, repoUrl);
            if (!node.second) {
                std::cerr << "repo " + repoName + " already exist." << std::endl;
                return -1;
            }

            auto ret = repo.setConfig(newCfg);
            if (!ret) {
                std::cerr << ret.error().message().toStdString() << std::endl;
                return -1;
            }

            return 0;
        }

        auto existingRepo = newCfg.repos.find(repoName);
        if (existingRepo == newCfg.repos.cend()) {
            std::cerr << "the operated repo " + repoName + " doesn't exist." << std::endl;
            return -1;
        }

        if (buildRepoRemove->parsed()) {
            if (newCfg.defaultRepo == repoName) {
                std::cerr << "repo " + repoName
                    + "is default repo, please change default repo before removing it.";
                return -1;
            }

            newCfg.repos.erase(existingRepo);
            auto ret = repo.setConfig(newCfg);
            if (!ret) {
                std::cerr << ret.error().message().toStdString() << std::endl;
                return -1;
            }

            return 0;
        }

        if (buildRepoUpdate->parsed()) {
            if (repoUrl.empty()) {
                std::cerr << "url is empty." << std::endl;
                return -1;
            }

            existingRepo->second = repoUrl;
            auto ret = repo.setConfig(newCfg);
            if (!ret) {
                std::cerr << ret.error().message().toStdString() << std::endl;
                return -1;
            }

            return 0;
        }

        if (buildRepoSetDefault->parsed()) {
            if (newCfg.defaultRepo != repoName) {
                newCfg.defaultRepo = repoName;
                auto ret = repo.setConfig(newCfg);
                if (!ret) {
                    std::cerr << ret.error().message().toStdString() << std::endl;
                    return -1;
                }
            }
            return 0;
        }

        std::cerr << "unknown operation, please see help information." << std::endl;
        return EINVAL;
    }

    if (buildRun->parsed()) {
        auto project =
          parseProjectConfig(QDir().absoluteFilePath(QString::fromStdString(filePath)));
        if (!project) {
            qCritical() << project.error();
            return -1;
        }

        linglong::builder::Builder builder(*project,
                                           QDir::current(),
                                           repo,
                                           *containerBuilder,
                                           *builderCfg);
        QStringList exec;
        QStringList modules = { "binary" };
        if (!newCommands.empty()) {
            auto execVerbose = commandList.join(" ");
            exec = splitExec(execVerbose);
        }

        if (!execModules.empty()) {
            for (const std::string &module : execModules) {
                modules.append(QString::fromStdString(module));
            }
        }

        bool debug = false;
        if (debugMode) {
            modules.push_back("develop");
            debug = true;
        }

        if (buildOffline) {
            auto cfg = builder.getConfig();
            options.skipFetchSource = true;
            options.skipPullDepend = true;
            cfg.offline = true;
            builder.setConfig(cfg);
            builder.setBuildOptions(options);
        }

        auto result = builder.run(modules, exec, debug);
        if (!result) {
            qCritical() << result.error();
            return -1;
        }

        return 0;
    }

    if (buildExport->parsed()) {
        auto project =
          parseProjectConfig(QDir().absoluteFilePath(QString::fromStdString(filePath)));
        if (!project) {
            qCritical() << project.error();
            return -1;
        }

        linglong::builder::Builder builder(*project,
                                           QDir::current(),
                                           repo,
                                           *containerBuilder,
                                           *builderCfg);

        if (layerMode) {
            auto result = builder.exportLayer(QDir::currentPath());
            if (!result) {
                qCritical() << result.error();
                return -1;
            }

            return 0;
        }

        auto result = builder.exportUAB(QDir::currentPath(),
                                        { .iconPath = QString::fromStdString(iconFile),
                                          .exportDevelop = true,
                                          .exportI18n = true });
        if (!result) {
            qCritical() << result.error();
            return -1;
        }

        return 0;
    }

    if (buildExtract->parsed()) {
        auto result = linglong::builder::Builder::extractLayer(QString::fromStdString(layerFile),
                                                               QString::fromStdString(dir));
        if (!result) {
            qCritical() << result.error();
            return -1;
        }

        return 0;
    }

    if (buildImport->parsed()) {
        auto result =
          linglong::builder::Builder::importLayer(repo, QString::fromStdString(layerFile));
        if (!result) {
            qCritical() << result.error();
            return -1;
        }
        return 0;
    }

    if (buildPush->parsed()) {
        auto project =
          parseProjectConfig(QDir().absoluteFilePath(QString::fromStdString(filePath)));
        if (!project) {
            qCritical() << project.error();
            return -1;
        }

        linglong::builder::Builder builder(*project,
                                           QDir::current(),
                                           repo,
                                           *containerBuilder,
                                           *builderCfg);
        if (!pushModule.empty()) {
            auto result = builder.push(pushModule, repoUrl, repoName);
            if (!result) {
                qCritical() << result.error();
                return -1;
            }
            return 0;
        }
        std::list<std::string> modules = { "binary", "develop" };
        if (project->modules.has_value()) {
            for (const auto &module : project->modules.value()) {
                if (module.name == "develop") {
                    continue;
                }
                modules.push_back(module.name);
            }
        }
        for (const auto &module : modules) {
            auto result = builder.push(module, repoUrl, repoName);
            if (!result) {
                qCritical() << result.error();
                return -1;
            }
        }

        return 0;
    }

    std::cout << commandParser.help("", CLI::AppFormatMode::All);

    return 0;
}
