/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/cli/interactive_notifier.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"

#include <CLI/App.hpp>

namespace linglong::cli {

struct CliOptions
{
    CliOptions(const std::string &filePath,
               const std::string &fileUrl,
               const std::string &workDir,
               const std::string &appid,
               const std::string &pagoda,
               const std::string &tier,
               const std::string &module,
               const std::string &type,
               const std::string &repoName,
               const std::string &repoUrl,
               std::vector<std::string> commands,
               bool showDevel)
        : filePath(filePath)
        , fileUrl(fileUrl)
        , workDir(workDir)
        , appid(appid)
        , pagoda(pagoda)
        , tier(tier)
        , module(module)
        , type(type)
        , repoName(repoName)
        , repoUrl(repoUrl)
        , commands(commands)
        , showDevel(showDevel){};

    CliOptions() = default;
    ~CliOptions() = default;

    std::string filePath;
    std::string fileUrl;
    std::string workDir;
    std::string appid;
    std::string pagoda;
    std::string tier;
    std::string module;
    std::string type;
    std::string repoName;
    std::string repoUrl;
    std::vector<std::string> commands;
    bool showDevel = false;
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
        runtime::ContainerBuilder &containerBuidler,
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
    int migrate();

    void cancelCurrentTask();

    void setCliOptions(const CliOptions &options) noexcept { this->options = options; }

private:
    int installFromFile(const QFileInfo &fileInfo);
    std::vector<std::string>
    filePathMapping(const std::vector<std::string> &command) const noexcept;
    static void filterPackageInfosFromType(std::vector<api::types::v1::PackageInfoV2> &list,
                                           const std::string &type) noexcept;
    void updateAM() noexcept;

private Q_SLOTS:
    void processDownloadStatus(const QString &recTaskID,
                               const QString &percentage,
                               const QString &message,
                               int status);
    void forwardMigrateDone(int code, QString message);

Q_SIGNALS:
    void migrateDone(int code, QString message, QPrivateSignal);

private:
    Printer &printer;
    ocppi::cli::CLI &ociCLI;
    runtime::ContainerBuilder &containerBuilder;
    repo::OSTreeRepo &repository;
    std::unique_ptr<InteractiveNotifier> notifier;
    api::dbus::v1::PackageManager &pkgMan;
    QString taskID;
    bool taskDone{ true };
    service::InstallTask::Status lastStatus;
    linglong::cli::CliOptions options;
};

} // namespace linglong::cli
