/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/printer/printer.h"
#include "linglong/util/command_helper.h"
#include "linglong/utils/error/error.h"

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

static qint64 systemHelperPid = -1;

static void handleOnExit(int, void *)
{
    if (systemHelperPid != -1) {
        kill(systemHelperPid, SIGTERM);
    }
}

static void startDaemon(QString program, QStringList args = {}, qint64 *pid = nullptr)
{
    QProcess process;
    process.setProgram(program);
    process.setStandardOutputFile("/dev/null");
    process.setStandardErrorFile("/dev/null");
    process.setArguments(args);
    process.startDetached(pid);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    using namespace linglong::utils::global;

    applicationInitializte();

    registerDBusParam();

    DocOptMap args = docopt::docopt(USAGE,
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

    std::unique_ptr<Printer> p;
    if (args["--json"].asBool()) {
        p = std::make_unique<PrinterJson>();
    } else {
        p = std::make_unique<PrinterNormal>();
    }

    auto appfn = std::make_shared<Factory<linglong::api::v1::dbus::AppManager1>>([]() {
        return std::make_shared<linglong::api::v1::dbus::AppManager1>(
          "org.deepin.linglong.AppManager",
          "/org/deepin/linglong/AppManager",
          QDBusConnection::sessionBus());
    });

    auto pkgfn = std::make_shared<Factory<linglong::api::v1::dbus::PackageManager1>>([]() {
        return std::make_shared<linglong::api::v1::dbus::PackageManager1>(
          "org.deepin.linglong.PackageManager",
          "/org/deepin/linglong/PackageManager",
          QDBusConnection::systemBus());
    });

    auto cmdhelperfn = std::make_shared<Factory<linglong::util::CommandHelper>>([]() {
        return std::make_shared<linglong::util::CommandHelper>();
    });

    auto pkgmngfn = std::make_shared<Factory<linglong::service::PackageManager>>([]() {
        return std::make_shared<linglong::service::PackageManager>();
    });

    Cli cli(args, std::move(p), appfn, pkgfn, cmdhelperfn, pkgmngfn);

    QMap<QString, std::function<int()>> subcommandMap = {
        { "run",
          [&cli]() {
              return cli.run();
          } },
        { "exec",
          [&cli]() {
              return cli.exec();
          } },
        { "enter",
          [&cli]() {
              return cli.enter();
          } },
        { "ps",
          [&cli]() {
              return cli.ps();
          } },
        { "kill",
          [&cli]() {
              return cli.kill();
          } },
        { "install",
          [&cli]() {
              return cli.install();
          } },
        { "upgrade",
          [&cli]() {
              return cli.upgrade();
          } },
        { "search",
          [&cli]() {
              return cli.search();
          } },
        { "uninstall",
          [&cli]() {
              return cli.uninstall();
          } },
        { "list",
          [&cli]() {
              return cli.list();
          } },
        { "repo",
          [&cli]() {
              return cli.repo();
          } },
    };

    for (const auto &subcommand : subcommandMap.keys()) {
        if (args[subcommand.toStdString()].asBool() == true) {
            return subcommandMap[subcommand]();
        }
    }
}
