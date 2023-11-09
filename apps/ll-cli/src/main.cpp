/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/api/dbus/v1/app_manager.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/json_printer.h"
#include "linglong/package_manager/package_manager.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>

using namespace linglong::utils::error;
using namespace linglong::package;
using namespace linglong::cli;

namespace {

qint64 systemHelperPid = -1;

void handleOnExit(int, void *)
{
    if (systemHelperPid != -1) {
        kill(systemHelperPid, SIGTERM);
    }
}

void startDaemon(QString program, QStringList args = {}, qint64 *pid = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setStandardOutputFile("/dev/null");
    process.setStandardErrorFile("/dev/null");
    process.setArguments(args);
    process.startDetached(pid);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    std::map<std::string, docopt::value> args =
      docopt::docopt(Cli::USAGE,
                     { argv + 1, argv + argc },
                     true,                  // show help if requested
                     "linglong CLI 1.4.0"); // version string

    auto systemHelperDBusConnection = QDBusConnection::systemBus();
    const auto systemHelperAddress = QString("unix:path=/run/linglong_system_helper_socket");

    if (args["--no-dbus"].asBool()) {
        on_exit(handleOnExit, nullptr);
        // NOTE: isConnected will NOT RETRY
        // NOTE: name cannot be duplicate
        systemHelperDBusConnection =
          QDBusConnection::connectToPeer(systemHelperAddress, "ll-system-helper-1");
        if (!systemHelperDBusConnection.isConnected()) {
            startDaemon("ll-system-helper", { "--bus=" + systemHelperAddress }, &systemHelperPid);
            QThread::sleep(1);
            systemHelperDBusConnection =
              QDBusConnection::connectToPeer(systemHelperAddress, "ll-system-helper");
            if (!systemHelperDBusConnection.isConnected()) {
                qCritical() << "failed to start ll-system-helper";
                exit(-1);
            }
        }
        setenv("LINGLONG_SYSTEM_HELPER_ADDRESS", systemHelperAddress.toStdString().c_str(), true);
    }

    std::unique_ptr<Printer> printer;
    if (args["--json"].asBool()) {
        printer = std::make_unique<JSONPrinter>();
    } else {
        printer = std::make_unique<Printer>();
    }

    auto appMan =
      std::make_unique<linglong::api::dbus::v1::AppManager>("org.deepin.linglong.AppManager",
                                                            "/org/deepin/linglong/AppManager",
                                                            QDBusConnection::sessionBus());
    auto pkgMan = std::make_unique<linglong::api::dbus::v1::PackageManager>(
      "org.deepin.linglong.PackageManager",
      "/org/deepin/linglong/PackageManager",
      QDBusConnection::systemBus());

    auto pkgManImpl = std::make_unique<linglong::service::PackageManager>();

    auto cli = std::make_unique<Cli>(*printer, *appMan, *pkgMan, *pkgManImpl);

    QMap<QString, std::function<int(Cli *, std::map<std::string, docopt::value> &)>>
      subcommandMap = {
          { "run", &Cli::run },
          { "exec", &Cli::exec },
          { "enter", &Cli::enter },
          { "ps", &Cli::ps },
          { "kill", &Cli::kill },
          { "install", &Cli::install },
          { "upgrade", &Cli::upgrade },
          { "search", &Cli::search },
          { "uninstall", &Cli::uninstall },
          { "list", &Cli::list },
          { "repo", &Cli::repo },
      };

    for (const auto &subcommand : subcommandMap.keys()) {
        if (args[subcommand.toStdString()].asBool() == true) {
            return subcommandMap[subcommand](cli.get(), args);
        }
    }
}
