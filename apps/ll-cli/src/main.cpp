/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/json_printer.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <cstddef>
#include <functional>
#include <memory>

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

          std::map<std::string, docopt::value> args =
            docopt::docopt(Cli::USAGE,
                           raw_args,
                           true,                              // show help if requested
                           "linglong CLI " LINGLONG_VERSION); // version string

          auto pkgManConn = QDBusConnection::systemBus();
          auto pkgMan =
            new linglong::api::dbus::v1::PackageManager("org.deepin.linglong.PackageManager",
                                                        "/org/deepin/linglong/PackageManager",
                                                        pkgManConn,
                                                        QCoreApplication::instance());

          if (args["--no-dbus"].asBool()) {
              if (getuid() != 0) {
                  qCritical() << "--no-dbus should only be used by root user.";
                  QCoreApplication::exit(-1);
                  return;
              }

              qInfo() << "some subcommands will failed in --no-dbus mode.";

              const auto pkgManAddress = QString("unix:path=/tmp/linglong-package-manager.socket");

              QThread::sleep(1);

              startProcess("sudo",
                           { "--user",
                             LINGLONG_USERNAME,
                             "--preserve-env=QT_FORCE_STDERR_LOGGING",
                             "--preserve-env=QDBUS_DEBUG",
                             LINGLONG_LIBEXEC_DIR "/ll-package-manager",
                             "--no-dbus" });
              QThread::sleep(1);

              pkgManConn = QDBusConnection::connectToPeer(pkgManAddress, "ll-package-manager");
              if (!pkgManConn.isConnected()) {
                  qCritical() << "Failed to connect to ll-package-manager:"
                              << pkgManConn.lastError();
                  QCoreApplication::exit(-1);
                  return;
              }

              pkgMan =
                new linglong::api::dbus::v1::PackageManager("",
                                                            "/org/deepin/linglong/PackageManager",
                                                            pkgManConn,
                                                            QCoreApplication::instance());
          } else {
              // NOTE: We need to ping package manager to make it initialize system linglong
              // repository.
              auto peer = linglong::api::dbus::v1::DBusPeer("org.deepin.linglong.PackageManager",
                                                            "/org/deepin/linglong/PackageManager",
                                                            pkgManConn);
              auto reply = peer.Ping();
              reply.waitForFinished();
              if (!reply.isValid()) {
                  qCritical() << "Failed to activate org.deepin.linglong.PackageManager"
                              << reply.error();
                  QCoreApplication::exit(-1);
                  return;
              }
          }

          std::unique_ptr<Printer> printer;
          if (args["--json"].asBool()) {
              printer = std::make_unique<JSONPrinter>();
          } else {
              printer = std::make_unique<Printer>();
          }

          auto config = linglong::repo::loadConfig(
            { LINGLONG_ROOT "/config.yaml", LINGLONG_DATA_DIR "/config.yaml" });
          if (!config) {
              qCritical() << config.error();
              QCoreApplication::exit(-1);
              return;
          }
          linglong::repo::ClientFactory clientFactory(config->repos[config->defaultRepo]);
          auto *repo = new linglong::repo::OSTreeRepo(QDir(LINGLONG_ROOT), *config, clientFactory);
          repo->setParent(QCoreApplication::instance());
          // 禁止cli自动更新ostree仓库配置
          repo->disableUpdateOstreeRepoConfig();

          auto ociRuntimeCLI = qgetenv("LINGLONG_OCI_RUNTIME");
          if (ociRuntimeCLI.isEmpty()) {
              ociRuntimeCLI = LINGLONG_DEFAULT_OCI_RUNTIME;
          }

          auto path = QStandardPaths::findExecutable(ociRuntimeCLI);
          if (path.isEmpty()) {
              qCritical() << ociRuntimeCLI << "not found";
              QCoreApplication::exit(-1);
              return;
          }
          auto ociRuntime = ocppi::cli::crun::Crun::New(path.toStdString());
          if (!ociRuntime) {
              std::rethrow_exception(ociRuntime.error());
          }
          auto containerBuidler = new linglong::runtime::ContainerBuilder(**ociRuntime);
          containerBuidler->setParent(QCoreApplication::instance());
          auto cli = new linglong::cli::Cli(*printer,
                                            **ociRuntime,
                                            *containerBuidler,
                                            *pkgMan,
                                            *repo,
                                            QCoreApplication::instance());

          QMap<QString, std::function<int(Cli *, std::map<std::string, docopt::value> &)>>
            subcommandMap = { { "run", &Cli::run },
                              { "exec", &Cli::exec },
                              { "enter", &Cli::exec },
                              { "ps", &Cli::ps },
                              { "kill", &Cli::kill },
                              { "install", &Cli::install },
                              { "upgrade", &Cli::upgrade },
                              { "search", &Cli::search },
                              { "uninstall", &Cli::uninstall },
                              { "list", &Cli::list },
                              { "repo", &Cli::repo },
                              { "info", &Cli::info },
                              { "content", &Cli::content } };

          if (!QObject::connect(QCoreApplication::instance(),
                                &QCoreApplication::aboutToQuit,
                                cli,
                                &Cli::cancelCurrentTask)) {
              qCritical() << "failed to connect signal: aboutToQuit";
              QCoreApplication::exit(-1);
              return;
          }

          for (const auto &subcommand : subcommandMap.keys()) {
              if (args[subcommand.toStdString()].asBool() == true) {
                  QCoreApplication::exit(subcommandMap[subcommand](cli, args));
                  return;
              }
          }
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
