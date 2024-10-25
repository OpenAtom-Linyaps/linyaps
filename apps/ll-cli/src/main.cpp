/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/dbus_notifier.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/json_printer.h"
#include "linglong/cli/terminal_notifier.h"
#include "linglong/repo/config.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/configure.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/global/initialize.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <sys/file.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>

#include <cstddef>
#include <functional>
#include <memory>
#include <thread>

#include <fcntl.h>
#include <unistd.h>
#include <wordexp.h>

using namespace linglong::utils::error;
using namespace linglong::package;
using namespace linglong::cli;

namespace {

void startProcess(const QString &program, const QStringList &args = {})
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
    res.emplace_back("--");

    for (size_t i = 0; i < words.we_wordc; i++) {
        res.emplace_back(words.we_wordv[i]);
    }

    QStringList list{};

    for (const auto &arg : res) {
        list.push_back(QString::fromStdString(arg));
    }

    qDebug() << "using new args" << list;
    return res;
}

void ensureDirectory(const std::filesystem::path &dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        if (ec) {
            qCritical() << QString{ "get status of" } << dir.c_str()
                        << "failed:" << ec.message().c_str();
            std::abort();
        }

        if (!std::filesystem::create_directory(dir, ec)) {
            qCritical() << "failed to create directory:" << ec.message().c_str();
            QCoreApplication::exit(ec.value());
            std::abort();
        }
    }
}

int lockCheck() noexcept
{
    std::error_code ec;
    constexpr auto lock = "/run/linglong/lock";
    if (!std::filesystem::exists(lock, ec)) {
        if (ec) {
            qCritical() << "failed to get status of" << lock;
            return -1;
        }

        return 0;
    }

    auto fd = ::open(lock, O_RDONLY);
    if (fd == -1) {
        qCritical() << "failed to open lock" << lock;
        return -1;
    }
    auto closeFd = linglong::utils::finally::finally([fd]() {
        ::close(fd);
    });

    struct flock lock_info
    {
        .l_type = F_RDLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0
    };

    if (::fcntl(fd, F_GETLK, &lock_info) == -1) {
        qCritical() << "failed to get lock" << lock;
        return -1;
    }

    if (lock_info.l_type == F_UNLCK) {
        return 0;
    }

    return lock_info.l_pid;
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
          auto repoRoot = QDir(LINGLONG_ROOT);
          if (!repoRoot.exists()) {
              qCritical() << "underlying repository doesn't exist:" << repoRoot.absolutePath();
              QCoreApplication::exit(-1);
              return;
          }

          auto userContainerDir =
            std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());
          ensureDirectory(userContainerDir);

          while (true) {
              auto lockOwner = lockCheck();
              if (lockOwner == -1) {
                  qCritical() << "lock check failed";
                  QCoreApplication::exit(-1);
                  return;
              }

              if (lockOwner > 0) {
                  qInfo() << "\r\33[K" << "\033[?25l"
                          << "repository is being operated by another process, waiting for"
                          << lockOwner << "\033[?25h";
                  using namespace std::chrono_literals;
                  std::this_thread::sleep_for(1s);
                  continue;
              }

              break;
          }

          auto raw_args = transformOldExec(argc, argv);
          std::map<std::string, docopt::value> args =
            docopt::docopt(Cli::USAGE,
                           raw_args,
                           true,                              // show help if requested
                           "linglong CLI " LINGLONG_VERSION); // version string
          auto pkgManConn = QDBusConnection::systemBus();

          auto msg = QDBusMessage::createMethodCall("org.deepin.linglong.PackageManager1",
                                                    "/org/deepin/linglong/Migrate1",
                                                    "org.deepin.linglong.Migrate1",
                                                    "WaitForAvailable");
          QDBusReply<void> migrateReply = pkgManConn.call(msg);
          if (migrateReply.isValid()) {
              qCritical()
                << "package manager is migrating underlying data, please try again later.";
              QCoreApplication::exit(-1);
              return;
          }

          auto *pkgMan =
            new linglong::api::dbus::v1::PackageManager("org.deepin.linglong.PackageManager1",
                                                        "/org/deepin/linglong/PackageManager1",
                                                        pkgManConn,
                                                        QCoreApplication::instance());
          bool noDBus = args["--no-dbus"].asBool();
          if (noDBus) {
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
                                                            "/org/deepin/linglong/PackageManager1",
                                                            pkgManConn,
                                                            QCoreApplication::instance());
          } else {
              // NOTE: We need to ping package manager to make it initialize system linglong
              // repository.
              auto peer = linglong::api::dbus::v1::DBusPeer("org.deepin.linglong.PackageManager1",
                                                            "/org/deepin/linglong/PackageManager1",
                                                            pkgManConn);
              auto reply = peer.Ping();
              reply.waitForFinished();
              if (!reply.isValid()) {
                  qCritical() << "Failed to activate org.deepin.linglong.PackageManager1"
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

          auto *repo = new linglong::repo::OSTreeRepo(repoRoot, *config, clientFactory);
          repo->setParent(QCoreApplication::instance());

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
          auto *containerBuidler = new linglong::runtime::ContainerBuilder(**ociRuntime);
          containerBuidler->setParent(QCoreApplication::instance());

          std::unique_ptr<InteractiveNotifier> notifier{ nullptr };

          // if ll-cli is running in tty, should use terminalNotifier.
          if (::isatty(STDIN_FILENO) != 0 && ::isatty(STDOUT_FILENO) != 0) {
              notifier = std::make_unique<TerminalNotifier>();
          } else {
              try {
                  notifier = std::make_unique<DBusNotifier>();
              } catch (std::runtime_error &err) {
                  qWarning() << "initialize DBus notifier error:" << err.what()
                             << "try to fallback to terminal notifier.";
              }
          }

          if (noDBus) {
              notifier.reset();
              notifier = std::make_unique<linglong::cli::DummyNotifier>();
          }

          if (!notifier) {
              qCritical() << "no available notifier, exit";
              QCoreApplication::exit(-1);
              return;
          }

          auto *cli = new linglong::cli::Cli(*printer,
                                             **ociRuntime,
                                             *containerBuidler,
                                             *pkgMan,
                                             *repo,
                                             std::move(notifier),
                                             QCoreApplication::instance());
          if (repo->needMigrate()) {
              auto nullArg = std::map<std::string, docopt::value>{};
              QCoreApplication::exit(cli->migrate(nullArg));
              return;
          }

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
                              { "content", &Cli::content },
                              { "migrate", &Cli::migrate },
                              { "prune", &Cli::prune } };

          if (QObject::connect(QCoreApplication::instance(),
                               &QCoreApplication::aboutToQuit,
                               cli,
                               &Cli::cancelCurrentTask)
              == nullptr) {
              qCritical() << "failed to connect signal: aboutToQuit";
              QCoreApplication::exit(-1);
              return;
          }

          for (const auto &subcommand : subcommandMap.keys()) {
              if (args[subcommand.toStdString()].asBool()) {
                  QCoreApplication::exit(subcommandMap[subcommand](cli, args));
                  return;
              }
          }
      },
      Qt::QueuedConnection);
    Q_ASSERT(ret);

    return QCoreApplication::exec();
}
