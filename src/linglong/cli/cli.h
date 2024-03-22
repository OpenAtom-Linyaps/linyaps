/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_CLI_CLI_H_
#define LINGLONG_CLI_CLI_H_

#include "linglong/api/dbus/v1/package_manager.h"
#include "linglong/cli/printer.h"
#include "linglong/package/package.h"
#include "linglong/package/ref.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/package_manager/task.h"
#include "linglong/runtime/dbus_proxy.h"
#include "linglong/service/app_manager.h"
#include "linglong/util/app_status.h"
#include "linglong/util/qserializer/json.h"
#include "linglong/util/status_code.h"
#include "linglong/util/sysinfo.h"
#include "linglong/util/xdg.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/global/initialize.h"

#include <docopt.h>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <csignal>
#include <cstddef>
#include <fstream>
#include <functional>
#include <memory>

namespace linglong::cli {

class Cli : public QObject
{
    Q_OBJECT
public:
    Cli(const Cli &) = delete;
    Cli(Cli &&) = delete;
    Cli &operator=(const Cli &) = delete;
    Cli &operator=(Cli &&) = delete;
    ~Cli() = default;
    Cli(Printer &printer,
        linglong::service::AppManager &appMan,
        const std::shared_ptr<linglong::api::dbus::v1::PackageManager>& pkgMan,
        QObject *parent = nullptr);

    static const char USAGE[];

private:
    Printer &printer;
    linglong::service::AppManager &appMan;
    std::shared_ptr<linglong::api::dbus::v1::PackageManager> pkgMan;
    QString taskID;
    bool taskDone{ false };
    service::InstallTask::Status lastStatus;
    utils::error::Result<QString> getContainerID(std::map<std::string, docopt::value> &args);
    void filePathMapping(std::map<std::string, docopt::value> &args,
                         const std::vector<std::string> &command,
                         QStringList &execOption) const noexcept;

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

    void cancelCurrentTask();

private Q_SLOTS:
    void processDownloadStatus(const QString &recTaskID,
                               const QString &percentage,
                               const QString &message,
                               int status);
};

} // namespace linglong::cli
#endif // LINGLONG_SRC_CLI_CLI_H
