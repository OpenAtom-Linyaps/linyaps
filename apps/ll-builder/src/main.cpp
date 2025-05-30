/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "command_options.h"
#include "configure.h"
#include "linglong/builder/config.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/cli/cli.h"
#include "linglong/package/architecture.h"
#include "linglong/package/version.h"
#include "linglong/repo/client_factory.h"
#include "linglong/repo/config.h"
#include "linglong/repo/migrate.h"
#include "linglong/utils/command/env.h"
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
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <wordexp.h>

namespace {

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

    for (const auto &configLocation : std::as_const(configLocations)) {
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

std::string validateNonEmptyString(const std::string &parameter)
{
    if (parameter.empty()) {
        return std::string{ _("Input parameter is empty, please input valid parameter instead") };
    }
    return {};
}

linglong::utils::error::Result<linglong::api::types::v1::BuilderProject>
parseProjectConfig(const std::filesystem::path &filename)
{
    LINGLONG_TRACE("parse project config " + QString::fromStdString(filename));
    std::cerr << "Using project file " + filename.string() << std::endl;
    auto project =
      linglong::utils::serialize::LoadYAMLFile<linglong::api::types::v1::BuilderProject>(filename);
    if (!project) {
        return project;
    }
    auto version =
      linglong::package::VersionV1::parse(QString::fromStdString(project->package.version));
    if (!version || !version->tweak) {
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

    // 校验bese和runtime版本是否合法
    auto baseFuzzyRef = linglong::package::FuzzyReference::parse(project->base.c_str());
    if (!baseFuzzyRef) {
        return LINGLONG_ERR("failed to parse base field", baseFuzzyRef);
    }
    auto ret = linglong::package::Version::validateDependVersion(baseFuzzyRef->version.value());
    if (!ret) {
        return LINGLONG_ERR("base version is not valid", ret);
    }
    if (project->runtime) {
        auto runtimeFuzzyRef =
          linglong::package::FuzzyReference::parse(project->runtime.value().c_str());
        if (!runtimeFuzzyRef) {
            return LINGLONG_ERR("failed to parse runtime field", runtimeFuzzyRef);
        }
        ret = linglong::package::Version::validateDependVersion(runtimeFuzzyRef->version.value());
        if (!ret) {
            return LINGLONG_ERR("runtime version is not valid", ret);
        }
    }
    return project;
}

linglong::utils::error::Result<std::filesystem::path>
getProjectYAMLPath(const std::filesystem::path &projectDir, const std::string &usePath)
{
    LINGLONG_TRACE("get project yaml path");

    std::error_code ec;
    if (!usePath.empty()) {
        std::filesystem::path path = std::filesystem::canonical(usePath, ec);
        if (ec) {
            return LINGLONG_ERR(QString("invalid file path %1 error: %2")
                                  .arg(usePath.c_str())
                                  .arg(ec.message().c_str()));
        }
        return path;
    }

    auto arch = linglong::package::Architecture::currentCPUArchitecture();
    if (arch && *arch != linglong::package::Architecture()) {
        std::filesystem::path path =
          projectDir / ("linglong." + arch->toString().toStdString() + ".yaml");
        if (std::filesystem::exists(path, ec)) {
            return path;
        }
        if (ec) {
            return LINGLONG_ERR(
              QString("path %1 error: %2").arg(path.c_str()).arg(ec.message().c_str()));
        }
    }

    std::filesystem::path path = projectDir / "linglong.yaml";
    if (std::filesystem::exists(path, ec)) {
        return path;
    }
    if (ec) {
        return LINGLONG_ERR(
          QString("path %1 error: %2").arg(path.c_str()).arg(ec.message().c_str()));
    }

    return LINGLONG_ERR("project yaml file not found");
}

int handleCreate(const CreateCommandOptions &options)
{
    qInfo() << "Handling create for project:" << QString::fromStdString(options.projectName);

    auto name = QString::fromStdString(options.projectName);
    QDir projectDir = QDir::current().absoluteFilePath(name);
    if (projectDir.exists()) {
        qCritical() << name << "project dir already exists";
        return -1;
    }

    auto ret = projectDir.mkpath(".");
    if (!ret) {
        qCritical() << "create project dir failed:" << projectDir.absolutePath();
        return -1;
    }

    auto configFilePath = projectDir.absoluteFilePath("linglong.yaml");
    const auto *templateFilePath = LINGLONG_DATA_DIR "/builder/templates/example.yaml";

    if (!QFileInfo::exists(templateFilePath)) {
        templateFilePath = ":/example.yaml"; // Use Qt resource fallback
        qInfo() << "Using template file from Qt resources:" << templateFilePath;
    } else {
        qInfo() << "Using template file from system path:" << templateFilePath;
    }

    QFile templateFile(templateFilePath);
    QFile configFile(configFilePath);

    if (!templateFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Failed to open template file:" << templateFilePath
                    << "Error:" << templateFile.errorString();
        return -1;
    }

    if (!configFile.open(QIODevice::WriteOnly)) {
        qCritical() << "Failed to open config file for writing:" << configFilePath
                    << "Error:" << configFile.errorString();
        return -1;
    }

    auto rawData = templateFile.readAll();
    rawData.replace("@ID@", name.toUtf8());

    if (configFile.write(rawData) <= 0) {
        qCritical() << "Failed to write config file:" << configFilePath
                    << "Error:" << configFile.errorString();
        return -1;
    }

    qInfo() << "Project" << name << "created successfully at" << projectDir.absolutePath();
    return 0;
}

int handleBuild(linglong::builder::Builder &builder, const BuildCommandOptions &options)
{
    qInfo() << "Handling build command";

    auto cfg = builder.getConfig();

    auto finalBuildOptions = options.builderSpecificOptions;

    // offline means skip fetch source and pull dependency
    if (options.buildOffline || cfg.offline) {
        finalBuildOptions.skipFetchSource = true;
        finalBuildOptions.skipPullDepend = true;
    }

    builder.setBuildOptions(finalBuildOptions);

    QStringList commandList;
    if (!options.commands.empty()) {
        for (const auto &command : options.commands) {
            commandList.append(QString::fromStdString(command));
        }
    }

    linglong::utils::error::Result<void> ret;
    if (!commandList.isEmpty()) {
        ret = builder.build(commandList);
    } else {
        ret = builder.build();
    }
    if (!ret) {
        qCritical() << "Build failed: " << ret.error();
        return ret.error().code();
    }

    qInfo() << "Build completed successfully.";

    return 0;
}

int handleRun(linglong::builder::Builder &builder, const RunCommandOptions &options)
{
    qInfo() << "Handling run command";

    QStringList modules = { "binary" };
    if (options.debugMode) {
        modules.push_back("develop");
    }
    if (!options.execModules.empty()) {
        for (const std::string &module : options.execModules) {
            modules.append(QString::fromStdString(module));
        }
    }
    modules.removeDuplicates(); // Ensure modules are unique

    QStringList commandList;
    if (!options.commands.empty()) {
        for (const auto &command : options.commands) {
            commandList.append(QString::fromStdString(command));
        }
    }

    auto result = builder.run(modules, commandList, options.debugMode);
    if (!result) {
        qCritical() << "Run failed: " << result.error();
        return result.error().code();
    }

    qInfo() << "Run completed successfully.";
    return 0;
}

int handleExport(linglong::builder::Builder &builder, const ExportCommandOptions &options)
{
    // Create a mutable copy of the export options to potentially modify defaults
    auto exportOpts = options.exportSpecificOptions;

    if (options.layerMode) {
        qInfo() << "Exporting as layer file...";
        // layer 默认使用lz4, 保持和之前版本的兼容
        if (exportOpts.compressor.empty()) {
            qInfo() << "Compressor not specified, defaulting to lz4 for layer export.";
            exportOpts.compressor = "lz4";
        }

        auto result = builder.exportLayer(exportOpts);
        if (!result) {
            qCritical() << "Export layer failed: " << result.error();
            return result.error().code();
        }
        qInfo() << "Layer export completed successfully.";
    } else {
        qInfo() << "Exporting as UAB file...";
        // uab 默认使用lz4可以更快解压速度，避免影响应用自运行
        if (exportOpts.compressor.empty()) {
            qInfo() << "Compressor not specified, defaulting to lz4 for UAB export.";
            exportOpts.compressor = "lz4";
        }

        auto result = builder.exportUAB(exportOpts, options.outputFile);
        if (!result) {
            qCritical() << "Export UAB failed: " << result.error();
            return result.error().code();
        }
        qInfo() << "UAB export completed successfully.";
    }

    return 0;
}

int handlePush(linglong::builder::Builder &builder, const PushCommandOptions &options)
{
    qInfo() << "Handling push command";
    const auto &repoOpts = options.repoOptions;

    for (const auto &module : options.pushModules) {
        qInfo() << "Pushing module:" << QString::fromStdString(module);

        auto result = builder.push(module, repoOpts.repoUrl, repoOpts.repoName);
        if (!result) {
            qCritical() << "Push failed for module" << QString::fromStdString(module) << ":"
                        << result.error();
            return result.error().code();
        }
        qInfo() << "Module" << QString::fromStdString(module) << "pushed successfully.";
    }

    qInfo() << "All modules pushed successfully.";
    return 0;
}

int handleList(linglong::repo::OSTreeRepo &repo, [[maybe_unused]] const ListCommandOptions &options)
{
    auto ret = linglong::builder::cmdListApp(repo);
    if (!ret.has_value()) {
        return -1;
    }
    return 0;
}

int handleRemove(linglong::repo::OSTreeRepo &repo, const RemoveCommandOptions &options)
{
    auto ret = linglong::builder::cmdRemoveApp(repo, options.removeList);
    if (!ret.has_value()) {
        return -1;
    }
    return 0;
}

int handleImport(linglong::repo::OSTreeRepo &repo, const ImportCommandOptions &options)
{
    QString layerFile = QString::fromStdString(options.layerFile);
    qInfo() << "Handling import command for layer file:" << layerFile;

    auto result = linglong::builder::Builder::importLayer(repo, layerFile);
    if (!result) {
        qCritical() << "Import layer failed: " << result.error();
        return result.error().code();
    }

    qInfo() << "Layer import completed successfully.";
    return 0;
}

int handleImportDir(linglong::repo::OSTreeRepo &repo, const ImportDirCommandOptions &options)
{
    QString layerDir = QString::fromStdString(options.layerDir);
    qInfo() << "Handling import-dir command for layer directory:" << layerDir;

    auto result = linglong::builder::Builder::importLayer(repo, layerDir);
    if (!result) {
        qCritical() << "Import layer directory failed: " << result.error();
        return result.error().code();
    }

    qInfo() << "Layer directory import completed successfully.";
    return 0;
}

int handleExtract(const ExtractCommandOptions &options)
{
    QString layerFile = QString::fromStdString(options.layerFile);
    QString targetDir = QString::fromStdString(options.dir);
    qInfo() << "Handling extract command for layer file:" << layerFile
            << "to directory:" << targetDir;

    auto result = linglong::builder::Builder::extractLayer(layerFile, targetDir);
    if (!result) {
        qCritical() << "Extract layer failed: " << result.error();
        return result.error().code();
    }

    qInfo() << "Layer extraction completed successfully.";
    return 0;
}

int handleRepoShow(linglong::repo::OSTreeRepo &repo)
{
    const auto &cfg = repo.getConfig();
    // Note: keep the same format as ll-cli repo
    size_t maxUrlLength = 0;
    for (const auto &r : cfg.repos) {
        maxUrlLength = std::max(maxUrlLength, r.url.size());
    }
    std::cout << "Default: " << cfg.defaultRepo << std::endl;
    std::cout << std::left << std::setw(11) << "Name";
    std::cout << std::setw(maxUrlLength + 2) << "Url" << std::setw(11) << "Alias" << std::endl;
    for (const auto &r : cfg.repos) {
        std::cout << std::left << std::setw(11) << r.name << std::setw(maxUrlLength + 2) << r.url
                  << std::setw(11) << r.alias.value_or(r.name) << std::endl;
    }
    return 0;
}

int handleRepoAdd(linglong::repo::OSTreeRepo &repo, linglong::cli::RepoOptions &options)
{
    auto newCfg = repo.getConfig();

    std::string alias = options.repoAlias.value_or(options.repoName);

    if (options.repoUrl.empty()) {
        std::cerr << "url is empty." << std::endl;
        return EINVAL;
    }

    bool isExist = std::any_of(newCfg.repos.begin(), newCfg.repos.end(), [&alias](const auto &r) {
        return r.alias.value_or(r.name) == alias;
    });
    if (isExist) {
        std::cerr << "repo " + alias + " already exist." << std::endl;
        return -1;
    }

    newCfg.repos.push_back(linglong::api::types::v1::Repo{
      .alias = options.repoAlias,
      .name = options.repoName,
      .url = options.repoUrl,
    });

    auto ret = repo.setConfig(newCfg);
    if (!ret) {
        std::cerr << ret.error().message().toStdString() << std::endl;
        return -1;
    }

    return 0;
}

int handleRepoRemove(linglong::repo::OSTreeRepo &repo, linglong::cli::RepoOptions &options)
{
    auto newCfg = repo.getConfig();
    const std::string &alias = options.repoAlias.value();

    auto existingRepo =
      std::find_if(newCfg.repos.begin(), newCfg.repos.end(), [&alias](const auto &r) {
          return r.alias.value_or(r.name) == alias;
      });

    if (existingRepo == newCfg.repos.cend()) {
        std::cerr << "the operated repo " + alias + " doesn't exist." << std::endl;
        return -1;
    }

    if (newCfg.defaultRepo == alias) {
        std::cerr << "repo " + alias
            + " is default repo, please change default repo before removing it."
                  << std::endl;
        return -1;
    }

    newCfg.repos.erase(existingRepo);
    auto ret = repo.setConfig(newCfg);
    if (!ret) {
        std::cerr << ret.error().message().toStdString() << std::endl;
        return -1;
    }

    qInfo() << "Repository" << QString::fromStdString(alias) << "removed successfully.";
    return 0;
}

int handleRepoUpdate(linglong::repo::OSTreeRepo &repo, linglong::cli::RepoOptions &options)
{
    auto newCfg = repo.getConfig();
    const std::string &alias = options.repoAlias.value();

    if (options.repoUrl.empty()) {
        std::cerr << "url is empty." << std::endl;
        return EINVAL;
    }

    auto existingRepo =
      std::find_if(newCfg.repos.begin(), newCfg.repos.end(), [&alias](const auto &r) {
          return r.alias.value_or(r.name) == alias;
      });

    if (existingRepo == newCfg.repos.cend()) {
        std::cerr << "the operated repo " + alias + " doesn't exist." << std::endl;
        return -1;
    }

    existingRepo->url = options.repoUrl;

    auto ret = repo.setConfig(newCfg);
    if (!ret) {
        std::cerr << ret.error().message().toStdString() << std::endl;
        return -1;
    }

    qInfo() << "Repository" << QString::fromStdString(alias) << "updated successfully.";
    return 0;
}

int handleRepoSetDefault(linglong::repo::OSTreeRepo &repo, linglong::cli::RepoOptions &options)
{
    auto newCfg = repo.getConfig();
    const std::string &alias = options.repoAlias.value();

    auto existingRepo =
      std::find_if(newCfg.repos.begin(), newCfg.repos.end(), [&alias](const auto &r) {
          return r.alias.value_or(r.name) == alias;
      });

    if (existingRepo == newCfg.repos.cend()) {
        std::cerr << "the operated repo " + alias + " doesn't exist." << std::endl;
        return -1;
    }

    if (newCfg.defaultRepo != alias) {
        newCfg.defaultRepo = alias;
        auto ret = repo.setConfig(newCfg);
        if (!ret) {
            std::cerr << ret.error().message().toStdString() << std::endl;
            return -1;
        }
        qInfo() << "Default repository set to" << QString::fromStdString(alias) << "successfully.";
    } else {
        qInfo() << QString::fromStdString(alias) << "is already the default repository.";
    }

    return 0;
}

int handleRepo(linglong::repo::OSTreeRepo &repo,
               const RepoSubcommandOptions &options,
               CLI::App *buildRepoShow,
               CLI::App *buildRepoAdd,
               CLI::App *buildRepoRemove,
               CLI::App *buildRepoUpdate,
               CLI::App *buildRepoSetDefault)
{
    if (buildRepoShow->parsed()) {
        return handleRepoShow(repo);
    }

    linglong::cli::RepoOptions repoOptions = options.repoOptions;
    if (!repoOptions.repoUrl.empty()) {
        if (repoOptions.repoUrl.rfind("http", 0) != 0) {
            std::cerr << "url is invalid." << std::endl;
            return EINVAL;
        }

        if (repoOptions.repoUrl.back() == '/') {
            repoOptions.repoUrl.pop_back();
        }
    }

    if (buildRepoAdd->parsed()) {
        return handleRepoAdd(repo, repoOptions);
    }

    if (buildRepoRemove->parsed()) {
        return handleRepoRemove(repo, repoOptions);
    }

    if (buildRepoUpdate->parsed()) {
        return handleRepoUpdate(repo, repoOptions);
    }

    if (buildRepoSetDefault->parsed()) {
        return handleRepoSetDefault(repo, repoOptions);
    }

    std::cerr << "unknown repo operation, please see help information." << std::endl;
    return EINVAL;
}

std::vector<std::string> getProjectModule(const linglong::api::types::v1::BuilderProject &project)
{
    std::list<std::string> modules = { "binary", "develop" }; // Start with base modules
    if (project.modules.has_value()) {
        for (const auto &moduleConfig : project.modules.value()) {
            // Add module name from the project config
            modules.push_back(moduleConfig.name);
        }
    }
    // Ensure uniqueness in the list
    modules.sort();
    modules.unique();

    // Convert the unique list to a vector for the return type
    return { modules.begin(), modules.end() };
}

std::optional<std::filesystem::path>
backupFailedMigrationRepo(const std::filesystem::path &repoPath)
{
    qWarning() << "Repository migration failed. Attempting to back up the old repository:"
               << QString::fromStdString(repoPath.string());

    auto backupDirPattern = (repoPath.parent_path() / "linglong-builder.old-XXXXXX").string();
    std::error_code ec;

    char *backupDir = ::mkdtemp(backupDirPattern.data());
    if (backupDir == nullptr) {
        qCritical() << "we couldn't generate a temporary directory for migrate, old repo will "
                       "be removed.";
        std::filesystem::remove_all(repoPath, ec); // Use remove_all for directories
        if (ec) {
            qCritical() << "failed to remove the old repo:" << QString::fromStdString(repoPath);
        }
        return std::nullopt;
    }

    std::filesystem::rename(repoPath, backupDir, ec);
    if (ec) {
        qCritical() << "Failed to move the old repository to the backup location (" << backupDir
                    << "). Error:" << ec.message().c_str() << "Please move or remove it manually:"
                    << QString::fromStdString(repoPath.string());
        // Attempt to clean up the created backup directory if rename failed
        return std::nullopt;
    }

    qInfo() << "Old repository successfully backed up to:" << backupDir
            << ". All data will need to be pulled again.";
    return backupDir;
}

} // namespace

int main(int argc, char **argv)
{
    bindtextdomain(PACKAGE_LOCALE_DOMAIN, PACKAGE_LOCALE_DIR);
    textdomain(PACKAGE_LOCALE_DOMAIN);
    QCoreApplication app(argc, argv);
    // 初始化 qt qrc
    Q_INIT_RESOURCE(builder_releases);
    // 初始化应用，builder在非tty环境也输出日志
    linglong::utils::global::applicationInitialize(true);

    CLI::App commandParser{ _("linyaps builder CLI \n"
                              "A CLI program to build linyaps application\n") };
    commandParser.get_help_ptr()->description(_("Print this help message and exit"));
    commandParser.set_help_all_flag("--help-all", _("Expand all help"));

    commandParser.usage(_("Usage: ll-builder [OPTIONS] [SUBCOMMAND]"));
    commandParser.footer([]() {
        return _(R"(If you found any problems during use
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues)");
    });

    CLI::Validator validatorString{ validateNonEmptyString, "" };

    CreateCommandOptions createOpts;
    BuildCommandOptions buildOpts;
    RunCommandOptions runOpts;
    ExportCommandOptions exportOpts;
    PushCommandOptions pushOpts;
    ListCommandOptions listOpts;
    RemoveCommandOptions removeOpts;
    ImportCommandOptions importOpts;
    ImportDirCommandOptions importDirOpts;
    ExtractCommandOptions extractOpts;
    RepoSubcommandOptions repoCmdOpts;

    // add builder flags
    bool versionFlag = false;
    commandParser.add_flag("--version", versionFlag, _("Show version"));

    // add builder create
    auto buildCreate =
      commandParser.add_subcommand("create", _("Create linyaps build template project"));
    buildCreate->usage(_("Usage: ll-builder create [OPTIONS] NAME"));
    buildCreate->add_option("NAME", createOpts.projectName, _("Project name"))
      ->required()
      ->check(validatorString);

    // add builder build
    std::string filePath;
    // group empty will hide command
    std::string hiddenGroup = "";
    auto buildBuilder = commandParser.add_subcommand("build", _("Build a linyaps project"));
    buildBuilder->usage(_("Usage: ll-builder build [OPTIONS] [COMMAND...]"));
    buildBuilder->add_option("-f, --file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    buildBuilder->add_option(
      "COMMAND",
      buildOpts.commands,
      _("Enter the container to execute command instead of building applications"));
    buildBuilder->add_flag("--offline",
                           buildOpts.buildOffline,
                           _("Only use local files. This implies --skip-fetch-source and "
                             "--skip-pull-depend will be set"));
    buildBuilder
      ->add_flag("--full-develop-module",
                 buildOpts.builderSpecificOptions.fullDevelop,
                 _("Build full develop packages, runtime requires"))
      ->group(hiddenGroup);
    buildBuilder->add_flag("--skip-fetch-source",
                           buildOpts.builderSpecificOptions.skipFetchSource,
                           _("Skip fetch sources"));
    buildBuilder->add_flag("--skip-pull-depend",
                           buildOpts.builderSpecificOptions.skipPullDepend,
                           _("Skip pull dependency"));
    buildBuilder->add_flag("--skip-run-container",
                           buildOpts.builderSpecificOptions.skipRunContainer,
                           _("Skip run container"));
    buildBuilder->add_flag("--skip-commit-output",
                           buildOpts.builderSpecificOptions.skipCommitOutput,
                           _("Skip commit build output"));
    buildBuilder->add_flag("--skip-output-check",
                           buildOpts.builderSpecificOptions.skipCheckOutput,
                           _("Skip output check"));
    buildBuilder->add_flag("--skip-strip-symbols",
                           buildOpts.builderSpecificOptions.skipStripSymbols,
                           _("Skip strip debug symbols"));
    buildBuilder->add_flag("--isolate-network",
                           buildOpts.builderSpecificOptions.isolateNetWork,
                           _("Build in an isolated network environment"));

    // add builder run
    auto buildRun = commandParser.add_subcommand("run", _("Run built linyaps app"));
    buildRun->usage(_("Usage: ll-builder run [OPTIONS] [COMMAND...]"));
    buildRun->add_option("-f, --file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    buildRun
      ->add_option("--modules",
                   runOpts.execModules,
                   _("Run specified module. eg: --modules binary,develop"))
      ->delimiter(',')
      ->type_name("modules");
    buildRun->add_option(
      "COMMAND",
      runOpts.commands,
      _("Enter the container to execute command instead of running application"));
    buildRun->add_flag("--debug",
                       runOpts.debugMode,
                       _("Run in debug mode (enable develop module)"));

    auto buildList = commandParser.add_subcommand("list", _("List built linyaps app"));
    buildList->usage(_("Usage: ll-builder list [OPTIONS]"));
    auto buildRemove = commandParser.add_subcommand("remove", _("Remove built linyaps app"));
    buildRemove->usage(_("Usage: ll-builder remove [OPTIONS] [APP...]"));
    buildRemove->add_option("APP", removeOpts.removeList);

    // build export
    auto *buildExport = commandParser.add_subcommand("export", _("Export to linyaps layer or uab"));
    buildExport->usage(_("Usage: ll-builder export [OPTIONS]"));

    buildExport->add_option("-f, --file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    buildExport
      ->add_option("-z, --compressor",
                   exportOpts.exportSpecificOptions.compressor,
                   "supported compressors are: lz4(default), lzma, zstd")
      ->type_name("X");
    auto *iconOpt =
      buildExport
        ->add_option("--icon", exportOpts.exportSpecificOptions.iconPath, _("Uab icon (optional)"))
        ->type_name("FILE")
        ->check(CLI::ExistingFile);
    auto *fullOpt =
      buildExport->add_flag("--full", exportOpts.exportSpecificOptions.full, _("Export uab fully"))
        ->group(hiddenGroup);
    auto *layerFlag =
      buildExport
        ->add_flag("--layer", exportOpts.layerMode, _("Export to linyaps layer file (deprecated)"))
        ->excludes(iconOpt, fullOpt);
    buildExport
      ->add_option("--loader", exportOpts.exportSpecificOptions.loader, _("Use custom loader"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile)
      ->excludes(layerFlag, fullOpt);
    buildExport
      ->add_flag("--no-develop",
                 exportOpts.exportSpecificOptions.noExportDevelop,
                 _("Don't export the develop module"))
      ->needs(layerFlag);
    buildExport->add_option("-o, --output", exportOpts.outputFile, _("Output file"))
      ->type_name("FILE")
      ->excludes(layerFlag);

    // build push
    std::string pushModule;
    auto *buildPush = commandParser.add_subcommand("push", _("Push linyaps app to remote repo"));
    buildPush->usage(_("Usage: ll-builder push [OPTIONS]"));
    buildPush->add_option("-f, --file", filePath, _("File path of the linglong.yaml"))
      ->type_name("FILE")
      ->check(CLI::ExistingFile);
    buildPush->add_option("--repo-url", pushOpts.repoOptions.repoUrl, _("Remote repo url"))
      ->type_name("URL")
      ->check(validatorString);
    buildPush->add_option("--repo-name", pushOpts.repoOptions.repoName, _("Remote repo name"))
      ->type_name("NAME")
      ->check(validatorString);
    buildPush->add_option("--module", pushModule, _("Push single module"))->check(validatorString);

    // add build import
    auto buildImport =
      commandParser.add_subcommand("import", _("Import linyaps layer to build repo"));
    buildImport->usage(_("Usage: ll-builder import [OPTIONS] LAYER"));
    buildImport->add_option("LAYER", importOpts.layerFile, _("Layer file path"))
      ->type_name("FILE")
      ->required()
      ->check(CLI::ExistingFile);

    // add build importDir
    auto buildImportDir =
      commandParser.add_subcommand("import-dir", _("Import linyaps layer dir to build repo"))
        ->group(hiddenGroup);
    buildImportDir->usage(_("Usage: ll-builder import-dir PATH"));
    buildImportDir->add_option("PATH", importDirOpts.layerDir, _("Layer dir path"))
      ->type_name("PATH")
      ->required();

    // add build extract
    auto buildExtract = commandParser.add_subcommand("extract", _("Extract linyaps layer to dir"));
    buildExtract->usage(_("Usage: ll-builder extract [OPTIONS] LAYER DIR"));
    buildExtract->add_option("LAYER", extractOpts.layerFile, _("Layer file path"))
      ->required()
      ->check(CLI::ExistingFile);
    buildExtract->add_option("DIR", extractOpts.dir, _("Destination directory"))
      ->type_name("DIR")
      ->required();

    // add build repo
    auto buildRepo = commandParser.add_subcommand("repo", _("Display and manage repositories"));
    buildRepo->usage(_("Usage: ll-builder repo [OPTIONS] SUBCOMMAND"));
    buildRepo->require_subcommand(1);

    // add repo sub command add
    auto buildRepoAdd = buildRepo->add_subcommand("add", _("Add a new repository"));
    buildRepoAdd->usage(_("Usage: ll-builder repo add [OPTIONS] NAME URL"));
    buildRepoAdd->add_option("NAME", repoCmdOpts.repoOptions.repoName, _("Specify the repo name"))
      ->required()
      ->check(validatorString);
    buildRepoAdd->add_option("URL", repoCmdOpts.repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);
    buildRepoAdd
      ->add_option("--alias", repoCmdOpts.repoOptions.repoAlias, _("Alias of the repo name"))
      ->type_name("ALIAS")
      ->check(validatorString);

    // add repo sub command remove
    auto buildRepoRemove = buildRepo->add_subcommand("remove", _("Remove a repository"));
    buildRepoRemove->usage(_("Usage: ll-builder repo remove [OPTIONS] NAME"));
    buildRepoRemove
      ->add_option("Alias", repoCmdOpts.repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto buildRepoUpdate = buildRepo->add_subcommand("update", _("Update the repository URL"));
    buildRepoUpdate->usage(_("Usage: ll-builder repo update [OPTIONS] NAME URL"));
    buildRepoUpdate
      ->add_option("Alias", repoCmdOpts.repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);
    buildRepoUpdate->add_option("URL", repoCmdOpts.repoOptions.repoUrl, _("Url of the repository"))
      ->required()
      ->check(validatorString);

    // add repo sub command update
    auto buildRepoSetDefault =
      buildRepo->add_subcommand("set-default", _("Set a default repository name"));
    buildRepoSetDefault->usage(_("Usage: ll-builder repo set-default [OPTIONS] NAME"));
    buildRepoSetDefault
      ->add_option("Alias", repoCmdOpts.repoOptions.repoAlias, _("Alias of the repo name"))
      ->required()
      ->check(validatorString);

    // add repo sub command show
    auto buildRepoShow = buildRepo->add_subcommand("show", _("Show repository information"));
    buildRepoShow->usage(_("Usage: ll-builder repo show [OPTIONS]"));

    CLI11_PARSE(commandParser, argc, argv);

    if (versionFlag) {
        std::cout << _("linyaps build tool version ") << LINGLONG_VERSION << std::endl;
        return 0;
    }

    if (buildCreate->parsed()) {
        return handleCreate(createOpts);
    }

    if (buildExtract->parsed()) {
        return handleExtract(extractOpts);
    }

    // following command need repo
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

    auto result = linglong::repo::tryMigrate(builderCfg->repo, *repoCfg);
    if (result == linglong::repo::MigrateResult::Failed) {
        if (!backupFailedMigrationRepo(builderCfg->repo)) {
            return -1;
        }
    }

    const auto defaultRepo = linglong::repo::getDefaultRepo(*repoCfg);
    linglong::repo::ClientFactory clientFactory(defaultRepo.url);
    auto repoRoot = QDir{ QString::fromStdString(builderCfg->repo) };
    if (!repoRoot.exists() && !repoRoot.mkpath(".")) {
        qCritical() << "failed to create the repository of builder.";
        return -1;
    }

    linglong::repo::OSTreeRepo repo(repoRoot, *repoCfg, clientFactory);

    if (buildRepo->parsed()) {
        return handleRepo(repo,
                          repoCmdOpts,
                          buildRepoShow,
                          buildRepoAdd,
                          buildRepoRemove,
                          buildRepoUpdate,
                          buildRepoSetDefault);
    }

    if (buildImport->parsed()) {
        return handleImport(repo, importOpts);
    }

    if (buildImportDir->parsed()) {
        return handleImportDir(repo, importDirOpts);
    }

    if (buildList->parsed()) {
        return handleList(repo, listOpts);
    }

    if (buildRemove->parsed()) {
        return handleRemove(repo, removeOpts);
    }

    // following command need builder
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

    auto *containerBuilder = new linglong::runtime::ContainerBuilder(**ociRuntime);
    containerBuilder->setParent(QCoreApplication::instance());

    // use the current directory as the project(working) directory
    std::error_code ec;
    auto projectDir = std::filesystem::current_path(ec);
    if (ec) {
        std::cerr << "invalid current directory: " << ec.message() << std::endl;
        return -1;
    }

    auto canonicalYamlPath = getProjectYAMLPath(projectDir, filePath);
    if (!canonicalYamlPath) {
        std::cerr << canonicalYamlPath.error().message().toStdString() << std::endl;
        return -1;
    }
    if (canonicalYamlPath->string().rfind(projectDir.string(), 0) != 0) {
        std::cerr << "the project file " << canonicalYamlPath->string()
                  << " is not under the current project directory " << projectDir.string();
        return -1;
    }

    auto project = parseProjectConfig(*canonicalYamlPath);
    if (!project) {
        qCritical() << project.error();
        return -1;
    }

    linglong::builder::Builder builder(*project,
                                       QDir(QString::fromStdString(projectDir)),
                                       repo,
                                       *containerBuilder,
                                       *builderCfg);
    builder.projectYamlFile = std::move(canonicalYamlPath).value();

    if (buildBuilder->parsed()) {
        return handleBuild(builder, buildOpts);
    }

    if (buildRun->parsed()) {
        return handleRun(builder, runOpts);
    }

    if (buildExport->parsed()) {
        return handleExport(builder, exportOpts);
    }

    if (buildPush->parsed()) {
        if (!pushModule.empty()) {
            pushOpts.pushModules = { pushModule };
        } else {
            pushOpts.pushModules = getProjectModule(*project);
        }
        return handlePush(builder, pushOpts);
    }

    std::cout << commandParser.help("", CLI::AppFormatMode::All);

    return 0;
}
