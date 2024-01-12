/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/api/dbus/v1/app_manager.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/json_printer.h"
#include "linglong/util/configure.h"
#include "linglong/utils/finally/finally.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <cstddef>
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>

#include <wordexp.h>

using namespace linglong::utils::error;
using namespace linglong::package;
using namespace linglong::cli;

namespace {

void startProcess(QString program, QStringList args = {})
{
    QProcess process;
    auto envs = process.environment();
    envs.push_back("QT_FORCE_STDERR_LOGGING=1");
    process.setEnvironment(envs);
    process.setProgram(program);
    process.setArguments(args);

    qint64 pid = 0;
    process.startDetached(&pid);

    qDebug() << "Start" << program << args << "as" << pid;

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [pid]() {
        qDebug() << "Kill" << pid;
        kill(pid, SIGTERM);
    });
}

std::vector<std::string> transformOldExec(int argc, char **argv) noexcept
{
    std::vector<std::string> res{ argv + 1, argv + argc };
    if (std::find(res.begin(), res.end(), "run") == res.end()) {
        return res;
    }

    auto exec = std::find(res.begin(), res.end(), "--exec");

    if (exec == res.end()) {
        return res;
    }

    if ((exec + 1) == res.end() || (exec + 2) != res.end()) {
        *exec = "--";
        qDebug() << "replace `--exec` with `--`";
        return res;
    }

    wordexp_t words;
    auto _ = linglong::utils::finally::finally([&]() {
        wordfree(&words);
    });

    auto ret = wordexp((exec + 1)->c_str(), &words, 0);
    if (ret) {
        qCritical() << "wordexp on" << (exec + 1)->c_str() << "failed with" << ret
                    << "transform old exec arguments failed.";
        return res;
    }

    res.erase(exec, res.end());
    res.push_back("--");

    for (size_t i = 0; i < words.we_wordc; i++) {
        res.push_back(words.we_wordv[i]);
    }

    QStringList list{};

    for (const auto &arg : res) {
        list.push_back(QString::fromStdString(arg));
    }

    qDebug() << "using new args" << list;
    return res;
}

} // namespace

using namespace linglong::utils::global;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    applicationInitializte();

    auto ret = QMetaObject::invokeMethod(
      QCoreApplication::instance(),
      [&argc, &argv]() {
          auto raw_args = transformOldExec(argc, argv);

          registerDBusParam();

          std::map<std::string, docopt::value> args =
            docopt::docopt(Cli::USAGE,
                           raw_args,
                           true,                              // show help if requested
                           "linglong CLI " LINGLONG_VERSION); // version string

          auto pkgManConn = QDBusConnection::systemBus();
          auto pkgMan = std::make_unique<linglong::api::dbus::v1::PackageManager>(
            "org.deepin.linglong.PackageManager",
            "/org/deepin/linglong/PackageManager",
            pkgManConn);

          auto appManConn = QDBusConnection::sessionBus();
          auto appMan =
            std::make_unique<linglong::api::dbus::v1::AppManager>("org.deepin.linglong.AppManager",
                                                                  "/org/deepin/linglong/AppManager",
                                                                  appManConn);

          if (args["--no-dbus"].asBool()) {
              if (getuid() != 0) {
                  qCritical() << "--no-dbus should only be used by root user.";
                  QCoreApplication::exit(-1);
                  return;
              }

              qInfo() << "some subcommands will failed in --no-dbus mode.";

              const auto pkgManAddress = QString("unix:path=/tmp/linglong-package-manager.socket");

              startProcess("ll-system-helper", { "--no-dbus" });
              QThread::sleep(1);

              startProcess("sudo",
                           { "--user",
                             LINGLONG_USERNAME,
                             "--preserve-env=QT_FORCE_STDERR_LOGGING",
                             "--preserve-env=QDBUS_DEBUG",
                             "ll-package-manager",
                             "--no-dbus" });
              QThread::sleep(1);

              pkgManConn = QDBusConnection::connectToPeer(pkgManAddress, "ll-package-manager");
              if (!pkgManConn.isConnected()) {
                  qCritical() << "Failed to connect to ll-package-manager:"
                              << pkgManConn.lastError();
                  QCoreApplication::exit(-1);
                  return;
              }

              pkgMan = std::make_unique<linglong::api::dbus::v1::PackageManager>(
                "",
                "/org/deepin/linglong/PackageManager",
                pkgManConn);
          }

          std::unique_ptr<Printer> printer;
          if (args["--json"].asBool()) {
              printer = std::make_unique<JSONPrinter>();
          } else {
              printer = std::make_unique<Printer>();
          }

          auto cli = std::make_unique<Cli>(*printer, *appMan, *pkgMan);

          QMap<QString, std::function<int(Cli *, std::map<std::string, docopt::value> &)>>
            subcommandMap = { { "run", &Cli::run },
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
                              { "info", &Cli::info } };

          for (const auto &subcommand : subcommandMap.keys()) {
              if (args[subcommand.toStdString()].asBool() == true) {
                  QCoreApplication::exit(subcommandMap[subcommand](cli.get(), args));
                  return;
              }
          }
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
