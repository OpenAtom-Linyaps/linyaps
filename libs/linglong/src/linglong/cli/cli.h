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
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/json.h"

#include <CLI/CLI.hpp>

namespace linglong::runtime {
class RunContext;
}

namespace linglong::generator {
class ContainerCfgBuilder;
}

namespace linglong::api::types::v1 {
struct PackageManager1InstallParameters;
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
    std::vector<std::string> extensions;
    bool privileged{ false };
    std::vector<std::string> capsAdd;
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
    bool depsOnly{ false };
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
    bool forceOpt{ false };
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
    std::string appid;
    std::string module;
    std::string dirType{ "layer" };
};

enum class TaskType : int {
    None,
    Install,
    InstallFromFile,
    Uninstall,
    Upgrade,
};

struct PMTaskState
{
    linglong::api::types::v1::State state{ linglong::api::types::v1::State::Unknown };
    std::string message;
    double percentage{ 0 };
    linglong::utils::error::ErrorCode errorCode;
    TaskType taskType{ TaskType::None };
    std::variant<api::types::v1::PackageManager1InstallParameters> params;
};

bool operator!=(const PMTaskState &lhs, const PMTaskState &rhs);
bool operator==(const PMTaskState &lhs, const PMTaskState &rhs);

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
    int inspect(CLI::App *subcommand, const InspectOptions &options);

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
    static void filterPackageInfosByVersion(
      std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list) noexcept;
    [[nodiscard]] utils::error::Result<std::vector<api::types::v1::CliContainer>>
    getCurrentContainers() const noexcept;
    int installFromFile(const QFileInfo &fileInfo,
                        const api::types::v1::CommonOptions &commonOptions,
                        const std::string &appid);
    int setRepoConfig(const QVariantMap &config);
    utils::error::Result<void> ensureAuthorized();
    utils::error::Result<void> runningAsRoot();
    utils::error::Result<void> runningAsRoot(const QList<QString> &args);
    utils::error::Result<std::vector<api::types::v1::UpgradeListResult>> listUpgradable();
    int generateCache(const package::Reference &ref);
    utils::error::Result<std::filesystem::path> ensureCache(
      runtime::RunContext &runContext, const generator::ContainerCfgBuilder &cfgBuilder) noexcept;
    QDBusReply<void> authorization();
    void updateAM() noexcept;
    std::vector<std::string> getRunningAppContainers(const std::string &appid);
    int getLayerDir(const InspectOptions &options);
    int getBundleDir(const InspectOptions &options);
    utils::error::Result<void> initInteraction();
    void detectDrivers();

    template <typename T>
    utils::error::Result<T> waitDBusReply(QDBusPendingReply<QVariantMap> &reply)
    {
        LINGLONG_TRACE("waitDBusReply");

        reply.waitForFinished();
        if (reply.isError()) {
            return LINGLONG_ERR(reply.error().message(), reply.error().type());
        }

        auto result = utils::serialize::fromQVariantMap<T>(reply.value());
        if (!result) {
            LogF("bug detected: {}", result.error());
            std::abort();
        }

        return result;
    }

    utils::error::Result<void> waitTaskCreated(QDBusPendingReply<QVariantMap> &reply,
                                               TaskType type);
    void waitTaskDone();

    void handleTaskState() noexcept;
    void handleInstallError(const utils::error::Error &error,
                            const api::types::v1::PackageManager1InstallParameters &params);
    void handleUninstallError(const utils::error::Error &error);
    void handleUpgradeError(const utils::error::Error &error);
    bool handleCommonError(const utils::error::Error &error);
    void printOnTaskFailed();
    void printOnTaskSuccess();

private Q_SLOTS:
    // maybe use in the future
    void onTaskAdded(const QDBusObjectPath &object_path);
    void onTaskRemoved(const QDBusObjectPath &object_path);
    void onTaskPropertiesChanged(const QString &interface,
                                 const QVariantMap &changed_properties,
                                 const QStringList &invalidated_properties);
    void interaction(const QDBusObjectPath &object_path,
                     int messageID,
                     const QVariantMap &additionalMessage);

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
    PMTaskState taskState;
    GlobalOptions globalOptions;
};

} // namespace linglong::cli
