/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/api/dbus/v1/task.h"
#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/api/types/v1/PackageInfoDisplay.hpp"
#include "linglong/api/types/v1/RepositoryCacheLayersItem.hpp"
#include "linglong/cli/interactive_notifier.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/error/error.h"

#include <CLI/App.hpp>

namespace linglong::runtime {
class RunContext;
}

namespace linglong::generator {
class ContainerCfgBuilder;
}

namespace linglong::cli {

class Printer;

// TODO: split this into multiple options
struct RepoOptions
{
    std::string repoName;
    std::string repoUrl;
    std::optional<std::string> repoAlias;
    std::int64_t repoPriority{ 0 };
};

struct CliOptions
{
    std::vector<std::string> filePaths;
    std::vector<std::string> fileUrls;
    std::string workDir;
    std::string appid;
    std::string instance;
    std::string module;
    std::string type;
    std::optional<std::string> repo;
    RepoOptions repoOptions;
    std::vector<std::string> commands;
    bool showDevel;
    bool showAllVersion;
    bool showUpgradeList;
    bool forceOpt;
    bool confirmOpt;
    std::optional<pid_t> pid;
    std::string signal;
    bool verbose;
};

class Cli : public QObject
{
    Q_OBJECT
public:
    Cli(const Cli &) = delete;
    Cli(Cli &&) = delete;
    Cli &operator=(const Cli &) = delete;
    Cli &operator=(Cli &&) = delete;
    ~Cli() override = default;
    Cli(Printer &printer,
        ocppi::cli::CLI &cli,
        runtime::ContainerBuilder &containerBuilder,
        api::dbus::v1::PackageManager &pkgMan,
        repo::OSTreeRepo &repo,
        std::unique_ptr<InteractiveNotifier> &&notifier,
        QObject *parent = nullptr);

    int run(CLI::App *subcommand);
    int exec(CLI::App *subcommand);
    int enter(CLI::App *subcommand);
    int ps(CLI::App *subcommand);
    int kill(CLI::App *subcommand);
    int install(CLI::App *subcommand);
    int upgrade(CLI::App *subcommand);
    int search(CLI::App *subcommand);
    int uninstall(CLI::App *subcommand);
    int list(CLI::App *subcommand);
    int repo(CLI::App *subcommand);
    int info(CLI::App *subcommand);
    int content(CLI::App *subcommand);
    int prune(CLI::App *subcommand);
    int inspect(CLI::App *subcommand);
    int dir(CLI::App *subcommand);

    void cancelCurrentTask();

    void setCliOptions(const CliOptions &options) noexcept { this->options = options; }

    void setCliOptions(CliOptions &&options) noexcept { this->options = std::move(options); }

private:
    [[nodiscard]] static utils::error::Result<void>
    RequestDirectories(const api::types::v1::PackageInfoV2 &info) noexcept;
    [[nodiscard]] std::vector<std::string>
    filePathMapping(const std::vector<std::string> &command) const noexcept;
    static std::string mappingFile(const std::filesystem::path &file) noexcept;
    static std::string mappingUrl(std::string_view url) noexcept;

    static void filterPackageInfosByType(
      std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list,
      const std::string &type) noexcept;
    static void filterPackageInfosByType(std::vector<api::types::v1::PackageInfoDisplay> &list,
                                         const std::string &type);
    static utils::error::Result<void> filterPackageInfosByVersion(
      std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list) noexcept;
    void printProgress() noexcept;
    [[nodiscard]] utils::error::Result<std::vector<api::types::v1::CliContainer>>
    getCurrentContainers() const noexcept;
    int installFromFile(const QFileInfo &fileInfo, const api::types::v1::CommonOptions &options);
    int setRepoConfig(const QVariantMap &config);
    utils::error::Result<void> runningAsRoot();
    utils::error::Result<void> runningAsRoot(const QList<QString> &args);
    utils::error::Result<std::vector<api::types::v1::UpgradeListResult>>
    listUpgradable(const std::vector<api::types::v1::PackageInfoV2> &pkgs);
    utils::error::Result<std::vector<api::types::v1::UpgradeListResult>>
    listUpgradable(const std::string &type = "app");
    int generateCache(const package::Reference &ref);
    utils::error::Result<std::filesystem::path> ensureCache(
      runtime::RunContext &runContext, const generator::ContainerCfgBuilder &cfgBuilder) noexcept;
    QDBusReply<QString> authorization();
    void updateAM() noexcept;

private Q_SLOTS:
    // maybe use in the future
    void onTaskAdded(QDBusObjectPath object_path);
    void onTaskRemoved(QDBusObjectPath object_path,
                       int state,
                       int subState,
                       QString message,
                       double percentage,
                       int code);
    void onTaskPropertiesChanged(QString interface,
                                 QVariantMap changed_properties,
                                 QStringList invalidated_properties);
    void interaction(QDBusObjectPath object_path, int messageID, QVariantMap additionalMessage);

Q_SIGNALS:
    void taskDone();

private:
    Printer &printer;
    ocppi::cli::CLI &ociCLI;
    runtime::ContainerBuilder &containerBuilder;
    repo::OSTreeRepo &repository;
    std::unique_ptr<InteractiveNotifier> notifier;
    api::dbus::v1::PackageManager &pkgMan;
    QString taskObjectPath;
    api::dbus::v1::Task1 *task{ nullptr };
    linglong::api::types::v1::State lastState{ linglong::api::types::v1::State::Unknown };
    linglong::api::types::v1::SubState lastSubState{ linglong::api::types::v1::SubState::Unknown };
    QString lastMessage;
    double lastPercentage{ 0 };
    linglong::utils::error::ErrorCode lastErrorCode;
    linglong::cli::CliOptions options;
};

} // namespace linglong::cli
