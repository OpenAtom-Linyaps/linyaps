/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/api/dbus/v1/app_manager.h"
#include "linglong/builder/builder.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/json_printer.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/configure.h"
#include "linglong/util/connection.h"
#include "linglong/utils/finally/finally.h"
#include "spdlog/logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/systemd_sink.h"

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

          linglong::api::client::ClientApi api;
          auto config =
            linglong::repo::loadConfig({ linglong::util::getLinglongRootPath() + "/config.yaml",
                                         LINGLONG_DATA_DIR "/config.yaml" });
          if (!config.has_value()) {
              qCritical() << config.error();
          }
          linglong::repo::OSTreeRepo ostree(linglong::util::getLinglongRootPath(), *config, api);

          std::unique_ptr<spdlog::logger> logger;
          {
              auto sinks = std::vector<std::shared_ptr<spdlog::sinks::sink>>(
                { std::make_shared<spdlog::sinks::systemd_sink_mt>("ocppi") });
              if (isatty(stderr->_fileno)) {
                  sinks.push_back(std::make_shared<spdlog::sinks::stderr_color_sink_mt>());
              }

              logger = std::make_unique<spdlog::logger>("ocppi", sinks.begin(), sinks.end());

              logger->set_level(spdlog::level::trace);
          }
          auto path = QStandardPaths::findExecutable("crun");
          auto crun = ocppi::cli::crun::Crun::New(path.toStdString(), logger);
          if (!crun.has_value()) {
              std::rethrow_exception(crun.error());
          }
          linglong::service::AppManager appManager(ostree, *crun->get());

          auto cli = std::make_unique<Cli>(*printer, appManager, *pkgMan);

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
