/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/api/dbus/v1/task.h"
#include "linglong/api/types/v1/CommonOptions.hpp"
#include "linglong/cli/interactive_notifier.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"

#include <CLI/App.hpp>

namespace linglong::cli {

class Printer;

struct CliOptions
{
    std::string filePath;
    std::string fileUrl;
    std::string workDir;
    std::string appid;
    std::string module;
    std::string type;
    std::string repoName;
    std::string repoUrl;
    std::vector<std::string> commands;
    bool showDevel;
    bool showUpgradeList;
    bool forceOpt;
    bool confirmOpt;
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

    int run();
    int exec();
    int enter();
    int ps();
    int kill();
    int install();
    int upgrade();
    int search();
    int uninstall();
    int list();
    int repo(CLI::App *app);
    int info();
    int content();
    int prune();

    void cancelCurrentTask();

    void setCliOptions(const CliOptions &options) noexcept { this->options = options; }

private:
    std::vector<std::string>
    filePathMapping(const std::vector<std::string> &command) const noexcept;
    static void filterPackageInfosFromType(std::vector<api::types::v1::PackageInfoV2> &list,
                                           const std::string &type) noexcept;
    void printProgress() noexcept;
    [[nodiscard]] utils::error::Result<std::vector<api::types::v1::CliContainer>>
    getCurrentContainers() const noexcept;
    int installFromFile(const QFileInfo &fileInfo, const api::types::v1::CommonOptions &options);
    int setRepoConfig(const QVariantMap &config);
    utils::error::Result<void> runningAsRoot();
    utils::error::Result<std::vector<api::types::v1::UpgradeListResult>>
    listUpgradable(const std::vector<api::types::v1::PackageInfoV2> &pkgs);
    utils::error::Result<std::vector<api::types::v1::UpgradeListResult>>
    listUpgradable(const std::string &type);

private Q_SLOTS:
    // maybe use in the future
    void onTaskAdded(QDBusObjectPath object_path);
    void onTaskRemoved(
      QDBusObjectPath object_path, int state, int subState, QString message, double percentage);
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
    linglong::cli::CliOptions options;
};

} // namespace linglong::cli
