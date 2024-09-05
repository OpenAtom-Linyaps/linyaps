/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"

#include <docopt.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>

namespace linglong::cli {

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
        QObject *parent = nullptr);

    static const char USAGE[];

private:
    Printer &printer;
    ocppi::cli::CLI &ociCLI;
    runtime::ContainerBuilder &containerBuilder;
    repo::OSTreeRepo &repository;
    api::dbus::v1::PackageManager &pkgMan;
    QString taskID;
    bool taskDone{ true };
    service::InstallTask::Status lastStatus;
    std::vector<std::string>
    filePathMapping(std::map<std::string, docopt::value> &args,
                    const std::vector<std::string> &command) const noexcept;
    static void filterPackageInfosFromType(std::vector<api::types::v1::PackageInfoV2> &list,
                                           const QString &type) noexcept;
    void updateAM() noexcept;
    [[nodiscard]] utils::error::Result<package::LayerDir> getDependLayerDir(
      const package::Reference &appRef, const package::Reference &ref) const noexcept;

public:
    int run(std::map<std::string, docopt::value> &args);
    int exec(std::map<std::string, docopt::value> &args);
    int enter(std::map<std::string, docopt::value> &args);
    int ps(std::map<std::string, docopt::value> &args);
    int kill(std::map<std::string, docopt::value> &args);
    int install(std::map<std::string, docopt::value> &args);
    int upgrade(std::map<std::string, docopt::value> &args);
    int search(std::map<std::string, docopt::value> &args);
    int uninstall(std::map<std::string, docopt::value> &args);
    int list(std::map<std::string, docopt::value> &args);
    int repo(std::map<std::string, docopt::value> &args);
    int info(std::map<std::string, docopt::value> &args);
    int content(std::map<std::string, docopt::value> &args);

    void cancelCurrentTask();

private Q_SLOTS:
    int installFromFile(const QFileInfo &fileInfo);
    void processDownloadStatus(const QString &recTaskID,
                               const QString &percentage,
                               const QString &message,
                               int status);
};

} // namespace linglong::cli
