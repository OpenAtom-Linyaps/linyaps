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

#include <CLI/CLI.hpp>

namespace linglong::runtime {
class RunContext;
}

namespace linglong::generator {
class ContainerCfgBuilder;
}

namespace linglong::cli {

class Printer;

// 全局选项（仅用于verbose等通用选项）
struct GlobalOptions
{
    bool verbose{ false };
};

// 各subcommand的独立选项结构体
struct RunOptions
{
    std::string appid;
    std::vector<std::string> filePaths;
    std::vector<std::string> fileUrls;
    std::vector<std::string> envs;
    std::vector<std::string> commands;
    std::optional<std::string> base;
    std::optional<std::string> runtime;
};

struct EnterOptions
{
    std::string instance;
    std::string workDir;
    std::vector<std::string> commands;
};

struct KillOptions
{
    std::string appid;
    std::string signal{ "SIGTERM" };
};

struct InstallOptions
{
    std::string appid;
    std::string module;
    std::optional<std::string> repo;
    bool forceOpt{ false };
    bool confirmOpt{ false };
};

struct UpgradeOptions
{
    std::string appid; // 可选，为空时升级所有应用
};

struct SearchOptions
{
    std::string appid; // 用作关键词
    std::string type{ "all" };
    std::optional<std::string> repo;
    bool showDevel{ false };
    bool showAllVersion{ false };
};

struct UninstallOptions
{
    std::string appid;
    std::string module;
};

struct ListOptions
{
    std::string type{ "all" };
    bool showUpgradeList{ false };
};

struct InfoOptions
{
    std::string appid;
};

struct ContentOptions
{
    std::string appid;
};

struct RepoOptions
{
    std::string repoName;
    std::string repoUrl;
    std::optional<std::string> repoAlias;
    std::int64_t repoPriority{ 0 };
};

struct InspectOptions
{
    std::optional<pid_t> pid;
};

struct DirOptions
{
    std::string appid;
    std::string module;
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
        ocppi::cli::CLI &ociCLI,
        runtime::ContainerBuilder &containerBuilder,
        api::dbus::v1::PackageManager &pkgMan,
        repo::OSTreeRepo &repo,
        std::unique_ptr<InteractiveNotifier> &&notifier,
        QObject *parent = nullptr);

    int run(const RunOptions &options);
    int enter(const EnterOptions &options);
    int ps();
    int kill(const KillOptions &options);
    int install(const InstallOptions &options);
    int upgrade(const UpgradeOptions &options);
    int search(const SearchOptions &options);
    int uninstall(const UninstallOptions &options);
    int list(const ListOptions &options);
    int repo(CLI::App *subcommand, const RepoOptions &options);
    int info(const InfoOptions &options);
    int content(const ContentOptions &options);
    int prune();
    int inspect(const InspectOptions &options);
    int dir(const DirOptions &options);

    void cancelCurrentTask();

    void setGlobalOptions(const GlobalOptions &options) noexcept { this->globalOptions = options; }

private:
    [[nodiscard]] static utils::error::Result<void>
    RequestDirectories(const api::types::v1::PackageInfoV2 &info) noexcept;
    [[nodiscard]] std::vector<std::string> filePathMapping(
      const std::vector<std::string> &command, const RunOptions &options) const noexcept;
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
    int installFromFile(const QFileInfo &fileInfo,
                        const api::types::v1::CommonOptions &commonOptions,
                        const std::string &appid);
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
    GlobalOptions globalOptions;
};

} // namespace linglong::cli
