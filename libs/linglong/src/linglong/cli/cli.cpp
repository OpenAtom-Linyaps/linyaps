/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/cli/cli.h"

#include "configure.h"
#include "linglong/api/dbus/v1/dbus_peer.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/api/types/v1/LinglongAPIV1.hpp"
#include "linglong/api/types/v1/PackageInfoDisplay.hpp"
#include "linglong/api/types/v1/PackageInfoV2.hpp"
#include "linglong/api/types/v1/PackageManager1InstallParameters.hpp"
#include "linglong/api/types/v1/PackageManager1JobInfo.hpp"
#include "linglong/api/types/v1/PackageManager1Package.hpp"
#include "linglong/api/types/v1/PackageManager1PackageTaskResult.hpp"
#include "linglong/api/types/v1/PackageManager1PruneResult.hpp"
#include "linglong/api/types/v1/PackageManager1SearchParameters.hpp"
#include "linglong/api/types/v1/PackageManager1SearchResult.hpp"
#include "linglong/api/types/v1/PackageManager1UninstallParameters.hpp"
#include "linglong/api/types/v1/State.hpp"
#include "linglong/cdi/cdi.h"
#include "linglong/cli/printer.h"
#include "linglong/common/dir.h"
#include "linglong/common/error.h"
#include "linglong/common/helper.h"
#include "linglong/common/socket.h"
#include "linglong/common/strings.h"
#include "linglong/package/layer_file.h"
#include "linglong/repo/config.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/runtime/run_context.h"
#include "linglong/utils/bash_command_helper.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/gettext.h"
#include "linglong/utils/namespace.h"
#include "linglong/utils/runtime_config.h"
#include "linglong/utils/xdp.h"
#include "ocppi/runtime/ExecOption.hpp"
#include "ocppi/runtime/RunOption.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <fmt/ranges.h>
#include <linux/un.h>
#include <nlohmann/json.hpp>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>

#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

using namespace linglong::utils::error;

namespace {

constexpr std::size_t ContainerIDDisplayLength = 12;

std::vector<std::string> getAutoModuleList() noexcept
{
    auto getModuleFromLanguageEnv = [](const std::string &lang) -> std::vector<std::string> {
        if (lang.length() < 2) {
            return { };
        }

        if (!std::all_of(lang.begin(), lang.begin() + 2, [](char c) {
                return 'a' <= c && c <= 'z';
            })) {
            return { };
        }

        std::vector<std::string> modules;
        modules.push_back("lang_" + lang.substr(0, 2));

        if (lang.length() == 2) {
            return modules;
        }

        if (lang[2] == '.') {
            return modules;
        }

        if (lang[2] == '@') {
            return modules;
        }

        if (lang[2] != '_') {
            return { };
        }

        if (lang.length() < 5) {
            return { };
        }

        modules.push_back("lang_" + lang.substr(0, 5));

        if (lang.length() == 5) {
            return modules;
        }

        if (lang[5] == '.') {
            return modules;
        }

        if (lang[5] == '@') {
            return modules;
        }

        return { };
    };

    auto envs = {
        "LANG",           "LC_ADDRESS",  "LC_ALL",       "LC_IDENTIFICATION",
        "LC_MEASUREMENT", "LC_MESSAGES", "LC_MONETARY",  "LC_NAME",
        "LC_NUMERIC",     "LC_PAPER",    "LC_TELEPHONE", "LC_TIME",
    };

    std::vector<std::string> result = { "binary" };

    for (const auto &env : envs) {
        auto lang = getenv(env);
        if (lang == nullptr) {
            continue;
        }
        auto modules = getModuleFromLanguageEnv(lang);
        result.insert(result.end(), modules.begin(), modules.end());
    }

    std::sort(result.begin(), result.end());
    return { result.begin(), std::unique(result.begin(), result.end()) };
}

} // namespace

namespace linglong::cli {

void Cli::onTaskPropertiesChanged(
  const QString &interface,                                   // NOLINT
  const QVariantMap &changed_properties,                      // NOLINT
  [[maybe_unused]] const QStringList &invalidated_properties) // NOLINT
{
    if (interface != task->interface()) {
        return;
    }

    for (auto entry = changed_properties.cbegin(); entry != changed_properties.cend(); ++entry) {
        const auto &key = entry.key();
        const auto &value = entry.value();

        if (key == "State") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                LogE("dbus ipc error, State couldn't convert to int");
                continue;
            }

            taskState.state = static_cast<api::types::v1::State>(val);
            continue;
        }

        if (key == "Percentage") {
            bool ok{ false };
            auto val = value.toDouble(&ok);
            if (!ok) {
                LogE("dbus ipc error, Percentage couldn't convert to int");
                continue;
            }

            taskState.percentage = val > 100 ? 100 : val;
            continue;
        }

        if (key == "Message") {
            if (!value.canConvert<QString>()) {
                LogE("dbus ipc error, Message couldn't convert to QString");
                continue;
            }

            taskState.message = value.toString().toStdString();
            continue;
        }

        if (key == "Code") {
            bool ok{ false };
            auto val = value.toInt(&ok);
            if (!ok) {
                LogE("dbus ipc error, Code couldn't convert to int");
                continue;
            }

            taskState.errorCode = static_cast<utils::error::ErrorCode>(val);
        }
    }

    handleTaskState();
}

void Cli::interaction(const QDBusObjectPath &object_path,
                      int messageID,
                      const QVariantMap &additionalMessage)
{
    LINGLONG_TRACE("interactive with user")
    if (object_path.path() != taskObjectPath) {
        return;
    }

    auto messageType = static_cast<api::types::v1::InteractionMessageType>(messageID);
    auto msg = common::serialize::fromQVariantMap<
      api::types::v1::PackageManager1RequestInteractionAdditionalMessage>(additionalMessage);

    std::vector<std::string> actions{ "yes", "Yes", "no", "No" };

    api::types::v1::InteractionRequest req;
    req.actions = actions;
    req.summary = "Package Manager needs to confirm request.";

    switch (messageType) {
    case api::types::v1::InteractionMessageType::Upgrade: {
        auto tips =
          QString("The lower version %1 is currently installed. Do you "
                  "want to continue installing the latest version %2?")
            .arg(QString::fromStdString(msg->localRef), QString::fromStdString(msg->remoteRef));
        req.body = tips.toStdString();
    } break;
    case api::types::v1::InteractionMessageType::Downgrade:
    case api::types::v1::InteractionMessageType::Install:
    case api::types::v1::InteractionMessageType::Uninstall:
        [[fallthrough]];
    case api::types::v1::InteractionMessageType::Unknown:
        // reserve for future use
        req.body = "unknown interaction type";
        break;
    }

    std::string action;
    auto notifyReply = notifier->request(req);
    if (!notifyReply) {
        LogE("internal error: notify failed");
        action = "no";
    } else {
        action = notifyReply->action.value();
    }

    // FIXME: if the notifier is a DummyNotifier, treat the action as yes.(for deepin-app-store)
    // But this behavior is no correct. We should treat it as no and tell people to add additional
    // option '-y'.
    if (action == "Y" || action == "y" || action == "Yes" || action == "yes" || action == "dummy") {
        action = "yes";
    } else {
        action = "no";
    }

    LogD("action: {}", action);

    auto reply = api::types::v1::InteractionReply{ .action = action };
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return;
    }

    QDBusPendingReply<void> dbusReply =
      (*pkgMan)->ReplyInteraction(object_path, common::serialize::toQVariantMap(reply));
    dbusReply.waitForFinished();
    if (dbusReply.isError()) {
        this->printer.printErr(
          LINGLONG_ERRV(dbusReply.error().message().toStdString(), dbusReply.error().type()));
    }
}

void Cli::onTaskAdded(const QDBusObjectPath &object_path)
{
    LogD("task added: {}", object_path.path().toStdString());
}

void Cli::onTaskRemoved(const QDBusObjectPath &object_path)
{
    LogD("task removed: {}", object_path.path().toStdString());
    if (object_path.path() != taskObjectPath) {
        return;
    }

    delete task;
    task = nullptr;
    Q_EMIT taskDone();
}

void Cli::handleTaskState() noexcept
{
    if (taskState.state == api::types::v1::State::Unknown) {
        LogW("task state is unknown");
        return;
    }

    if (taskState.state == api::types::v1::State::Failed
        || taskState.state == api::types::v1::State::Canceled) {
        this->printer.clearLine();
        this->printOnTaskFailed();
        return;
    }

    if (taskState.state == api::types::v1::State::Succeed) {
        this->printer.clearLine();
        this->printOnTaskSuccess();
        return;
    }

    if (!this->globalOptions.noProgress) {
        this->printer.printProgress(taskState.percentage, taskState.message);
    }
}

void Cli::printOnTaskFailed()
{
    LINGLONG_TRACE("cli handle task failed");

    auto error = LINGLONG_ERRV(taskState.message, taskState.errorCode);

    switch (taskState.taskType) {
    case TaskType::Install:
        handleInstallError(
          error,
          std::get<api::types::v1::PackageManager1InstallParameters>(taskState.params));
        break;
    case TaskType::Uninstall:
        handleUninstallError(error);
        break;
    case TaskType::Upgrade:
        handleUpgradeError(error);
        break;
    default:
        handleCommonError(error);
        break;
    }
}

void Cli::printOnTaskSuccess()
{
    this->printer.printMessage(taskState.message);
}

Cli::Cli(Printer &printer,
         ocppi::cli::CLI &ociCLI,
         runtime::ContainerBuilder &containerBuilder,
         bool peerMode,
         repo::OSTreeRepo &repo,
         std::unique_ptr<InteractiveNotifier> &&notifier,
         QObject *parent)
    : QObject(parent)
    , printer(printer)
    , ociCLI(ociCLI)
    , containerBuilder(containerBuilder)
    , repository(repo)
    , notifier(std::move(notifier))
    , peerMode(peerMode)
{
}

utils::error::Result<api::dbus::v1::PackageManager *> Cli::getPkgMan()
{
    LINGLONG_TRACE("get package manager");

    if (this->pkgMan) {
        return this->pkgMan.get();
    }

    if (this->peerMode) {
        if (getuid() != 0) {
            return LINGLONG_ERR("--no-dbus should only be used by root user.");
        }

        LogW("some subcommands will failed in --no-dbus mode.");
        const auto pkgManAddress = QString("unix:path=/tmp/linglong-package-manager.socket");
        QProcess::startDetached("sudo",
                                { "--user",
                                  LINGLONG_USERNAME,
                                  "--preserve-env=QT_FORCE_STDERR_LOGGING",
                                  "--preserve-env=QDBUS_DEBUG",
                                  LINGLONG_LIBEXEC_DIR "/ll-package-manager",
                                  "--no-dbus" });
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);

        const auto &pkgManConn =
          QDBusConnection::connectToPeer(pkgManAddress, "ll-package-manager");
        if (!pkgManConn.isConnected()) {
            return LINGLONG_ERR(fmt::format("Failed to connect to ll-package-manager: {}",
                                            pkgManConn.lastError().message().toStdString()));
        }

        this->pkgMan =
          std::make_unique<api::dbus::v1::PackageManager>("",
                                                          "/org/deepin/linglong/PackageManager1",
                                                          pkgManConn,
                                                          QCoreApplication::instance());
    } else {
        const auto &pkgManConn = QDBusConnection::systemBus();

        auto peer = linglong::api::dbus::v1::DBusPeer("org.deepin.linglong.PackageManager1",
                                                      "/org/deepin/linglong/PackageManager1",
                                                      pkgManConn);
        auto reply = peer.Ping();
        reply.waitForFinished();
        if (!reply.isValid()) {
            return LINGLONG_ERR(
              fmt::format("Failed to activate org.deepin.linglong.PackageManager1: {}",
                          reply.error().message().toStdString()));
        }

        this->pkgMan =
          std::make_unique<api::dbus::v1::PackageManager>("org.deepin.linglong.PackageManager1",
                                                          "/org/deepin/linglong/PackageManager1",
                                                          pkgManConn,
                                                          QCoreApplication::instance());
    }

    auto ret = this->initPkgManSignals();
    if (!ret) {
        this->pkgMan.reset();
        return LINGLONG_ERR("failed to initialize package manager signals", ret.error());
    }

    return this->pkgMan.get();
}

utils::error::Result<void> Cli::initPkgManSignals()
{
    LINGLONG_TRACE("init package manager signals");

    if (this->pkgManSignalsInitialized) {
        return LINGLONG_OK;
    }

    auto pkgManRet = this->getPkgMan();
    if (!pkgManRet) {
        return LINGLONG_ERR(pkgManRet);
    }
    auto *pkgMan = *pkgManRet;

    auto conn = pkgMan->connection();
    if (!conn.connect(pkgMan->service(),
                      pkgMan->path(),
                      pkgMan->interface(),
                      "TaskAdd",
                      this,
                      SLOT(onTaskAdded(QDBusObjectPath)))) {
        return LINGLONG_ERR("couldn't connect to package manager signal 'TaskAdded'");
    }

    if (!conn.connect(pkgMan->service(),
                      pkgMan->path(),
                      pkgMan->interface(),
                      "TaskRemoved",
                      this,
                      SLOT(onTaskRemoved(QDBusObjectPath)))) {
        return LINGLONG_ERR("couldn't connect to package manager signal 'TaskRemoved'");
    }

    this->pkgManSignalsInitialized = true;
    return LINGLONG_OK;
}

utils::error::Result<int> Cli::reuseContainer(const std::string &id,
                                              const std::vector<std::string> &commands) noexcept
{
    LINGLONG_TRACE(fmt::format("reuse container {}", id));

    LogD("attempting to reuse container");
    if (commands.empty()) {
        return LINGLONG_ERR("empty command");
    }

    auto containerLock = common::dir::getBundleDir(id) / ".lock";
    auto lockRet =
      utils::filelock::FileLock::create(containerLock, utils::filelock::LockType::Read, false);
    if (!lockRet) {
        // maybe the main container is initializing
        LogD("lock file {} doesn't exist", containerLock);
        return 0;
    }
    auto lock = std::move(lockRet).value();

    auto ret = lock.tryLock(utils::filelock::LockType::Read);
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (!*ret) {
        LogD("failed to acquire read lock: {}", containerLock);
        return 0;
    }

    {
        std::ifstream in(containerLock);
        std::string content;
        in >> content;
        if (content != "running") {
            LogD("container status not running, maybe initializing?");
            return 0;
        }
    }

    std::filesystem::path consoleSocket;
    auto remove = utils::finally::finally([&consoleSocket]() {
        if (!consoleSocket.empty()) {
            std::error_code ec;
            std::filesystem::remove(consoleSocket, ec);
            if (ec) {
                LogE("failed to remove console socket {}: {}", consoleSocket, ec.message());
            }
        }
    });

    auto closeFd = [](int &fd) {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    };

    auto setNonBlock = [](int fd) -> utils::error::Result<int> {
        LINGLONG_TRACE(fmt::format("set fd {} nonblock", fd));
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }

        if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }

        return flags;
    };

    struct NonTtyPipes
    {
        std::array<int, 2> stdinPipe{ -1, -1 };
        std::array<int, 2> stdoutPipe{ -1, -1 };
        std::array<int, 2> stderrPipe{ -1, -1 };
    };

    std::optional<NonTtyPipes> nonTtyPipes;
    int listenFd{ -1 };

    auto option = ocppi::runtime::ExecOption{ .uid = ::getuid(), .gid = ::getgid() };
    option.tty = false;
    if (isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0) {
        option.tty = true;

        consoleSocket =
          common::dir::getRuntimeDir() / ("socket-" + common::strings::generateRandomString(6));
        auto ret = common::socket::createUnixSocket(consoleSocket.c_str());
        if (!ret) {
            return LINGLONG_ERR(fmt::format("failed to create unix socket: {}", ret.error()));
        }

        listenFd = ret.value();
        option.extra.emplace_back(fmt::format("--console-socket={}", consoleSocket));
    } else {
        nonTtyPipes.emplace();
        if (::pipe2(nonTtyPipes->stdinPipe.data(), O_CLOEXEC) != 0
            || ::pipe2(nonTtyPipes->stdoutPipe.data(), O_CLOEXEC) != 0
            || ::pipe2(nonTtyPipes->stderrPipe.data(), O_CLOEXEC) != 0) {
            closeFd(nonTtyPipes->stdinPipe[0]);
            closeFd(nonTtyPipes->stdinPipe[1]);
            closeFd(nonTtyPipes->stdoutPipe[0]);
            closeFd(nonTtyPipes->stdoutPipe[1]);
            closeFd(nonTtyPipes->stderrPipe[0]);
            closeFd(nonTtyPipes->stderrPipe[1]);
            return LINGLONG_ERR(
              fmt::format("failed to create pipe: {}", common::error::errorString(errno)));
        }
    }

    auto releaseIoChannel = utils::finally::finally([&]() {
        closeFd(listenFd);
        if (nonTtyPipes) {
            closeFd(nonTtyPipes->stdinPipe[0]);
            closeFd(nonTtyPipes->stdinPipe[1]);
            closeFd(nonTtyPipes->stdoutPipe[0]);
            closeFd(nonTtyPipes->stdoutPipe[1]);
            closeFd(nonTtyPipes->stderrPipe[0]);
            closeFd(nonTtyPipes->stderrPipe[1]);
        }
    });

    sigset_t mask;
    sigset_t oldMask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGWINCH);
    if (sigprocmask(SIG_BLOCK, &mask, &oldMask) < 0) {
        return LINGLONG_ERR(
          fmt::format("failed to mask signal: {}", common::error::errorString(errno)));
    }

    auto restoreSigMask = utils::finally::finally([&oldMask]() {
        ::sigprocmask(SIG_SETMASK, &oldMask, nullptr);
    });

    auto child = ::fork();
    if (child == 0) {
        ::sigprocmask(SIG_SETMASK, &oldMask, nullptr);

        if (option.tty) {
            closeFd(listenFd);
        } else {
            closeFd(nonTtyPipes->stdinPipe[1]);
            closeFd(nonTtyPipes->stdoutPipe[0]);
            closeFd(nonTtyPipes->stderrPipe[0]);

            if (::dup2(nonTtyPipes->stdinPipe[0], STDIN_FILENO) == -1
                || ::dup2(nonTtyPipes->stdoutPipe[1], STDOUT_FILENO) == -1
                || ::dup2(nonTtyPipes->stderrPipe[1], STDERR_FILENO) == -1) {
                LogE("failed to dup pipe to stdio: {}", common::error::errorString(errno));
                _exit(EXIT_FAILURE);
            }

            closeFd(nonTtyPipes->stdinPipe[0]);
            closeFd(nonTtyPipes->stdoutPipe[1]);
            closeFd(nonTtyPipes->stderrPipe[1]);
        }

        auto reuseCommand = utils::BashCommandHelper::generateBashCommandBase();
        reuseCommand.push_back(utils::BashCommandHelper::generateEntrypointScript(commands));
        auto result = ociCLI.exec(id, "/run/linglong/container-init", reuseCommand, option);
        if (!result) {
            try {
                std::rethrow_exception(result.error());
            } catch (const std::exception &e) {
                LogE("failed to exec container: {}", LINGLONG_ERRV(result));
                _exit(EXIT_FAILURE);
            }
        }

        _exit(EXIT_SUCCESS);
    }

    if (child < 0) {
        return LINGLONG_ERR(fmt::format("failed to fork: {}", common::error::errorString(errno)));
    }

    auto sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0) {
        return LINGLONG_ERR(
          fmt::format("failed to create signalfd: {}", common::error::errorString(errno)));
    }
    auto closeSfd = utils::finally::finally([sfd]() {
        ::close(sfd);
    });

    bool reused{ false };
    bool childExited{ false };
    bool lockUnlocked{ false };
    int masterIn{ -1 };
    int masterOut{ -1 };
    int stdoutIn{ -1 };
    int stderrIn{ -1 };

    auto reapChild = [&]() {
        while (!childExited) {
            LogD("reap child");
            int status = 0;
            auto ret = ::waitpid(child, &status, WNOHANG);
            if (ret == child) {
                childExited = true;
                if (WIFEXITED(status)) {
                    auto exit_status = WEXITSTATUS(status);
                    LogD("oci runtime child {} exited with status {}", child, exit_status);
                    if (exit_status == 0) {
                        reused = true;
                    }
                } else if (WIFSIGNALED(status)) {
                    LogD("oci runtime child {} killed by signal {}", child, WTERMSIG(status));
                } else {
                    LogW("oci runtime child {} changed to unexpected status {}", child, status);
                }

                if (!lockUnlocked) {
                    auto unlockRet = lock.unlock();
                    if (!unlockRet) {
                        LogE("failed to unlock cli lock: {}", unlockRet.error());
                    }
                    lockUnlocked = true;
                }

                break;
            }

            if (ret == 0) {
                break;
            }

            if (ret == -1 && errno == EINTR) {
                continue;
            }

            if (ret == -1 && errno == ECHILD) {
                LogD("oci runtime child {} is not waitable: {}",
                     child,
                     common::error::errorString(errno));
                childExited = true;
                break;
            }

            LogW("failed to wait oci runtime child {}: {}",
                 child,
                 common::error::errorString(errno));
            break;
        }
    };

    auto drainSignals = [&]() {
        while (true) {
            LogD("drain signal");
            struct signalfd_siginfo info{ };
            auto n = ::read(sfd, &info, sizeof(info));
            if (n == sizeof(info)) {
                LogD("get sig :{}", info.ssi_signo);
                if (info.ssi_signo == SIGCHLD) {
                    reapChild();
                    continue;
                }

                if (info.ssi_signo == SIGWINCH && option.tty && masterIn != -1) {
                    struct winsize ws{ };
                    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
                        if (::ioctl(masterIn, TIOCSWINSZ, &ws) == 0) {
                            LogD("propagated terminal size to pty master {}", masterIn);
                        } else {
                            LogW("failed to set terminal size to pty master {}: {}",
                                 masterIn,
                                 common::error::errorString(errno));
                        }
                    } else {
                        LogW("failed to get terminal size: {}", common::error::errorString(errno));
                    }
                }

                continue;
            }

            if (n == -1 && errno == EINTR) {
                continue;
            }

            if (n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                LogW("failed to read signalfd: {}", common::error::errorString(errno));
            }

            break;
        }
    };

    if (option.tty) {
        if (auto ret = setNonBlock(listenFd); !ret) {
            return LINGLONG_ERR(ret);
        }

        auto acceptEpfd = ::epoll_create1(EPOLL_CLOEXEC);
        if (acceptEpfd < 0) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }
        auto closeAcceptEpfd = utils::finally::finally([acceptEpfd]() {
            ::close(acceptEpfd);
        });

        struct epoll_event ev{ };
        ev.events = EPOLLIN;
        ev.data.fd = listenFd;
        if (::epoll_ctl(acceptEpfd, EPOLL_CTL_ADD, listenFd, &ev) == -1) {
            return LINGLONG_ERR(fmt::format("failed to add console socket to epoll: {}",
                                            common::error::errorString(errno)));
        }
        ev.events = EPOLLIN;
        ev.data.fd = sfd;
        if (::epoll_ctl(acceptEpfd, EPOLL_CTL_ADD, sfd, &ev) == -1) {
            return LINGLONG_ERR(fmt::format("failed to add signalfd to epoll: {}",
                                            common::error::errorString(errno)));
        }

        std::array<struct epoll_event, 2> events{ };
        while (masterIn == -1 && !childExited) {
            auto nfds = ::epoll_wait(acceptEpfd, events.data(), events.size(), -1);
            if (nfds == -1) {
                if (errno == EINTR) {
                    continue;
                }

                return LINGLONG_ERR(
                  fmt::format("epoll_wait error: {}", common::error::errorString(errno)));
            }

            for (int i = 0; i < nfds; ++i) {
                if (events.at(i).data.fd == sfd) {
                    drainSignals();
                    continue;
                }

                struct sockaddr_un clientAddr{ };
                socklen_t addrLen = sizeof(clientAddr);
                auto client = ::accept4(listenFd,
                                        reinterpret_cast<struct sockaddr *>(&clientAddr),
                                        &addrLen,
                                        SOCK_CLOEXEC);
                if (client < 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }

                    return LINGLONG_ERR(common::error::errorString(errno));
                }

                auto closeClient = utils::finally::finally([client]() {
                    ::close(client);
                });

                auto data = common::socket::recvFdWithPayload(client);
                if (!data) {
                    return LINGLONG_ERR(data.error());
                }

                masterIn = data->fd;

                struct stat buf{ };
                fstat(masterIn, &buf);
                if (!S_ISCHR(buf.st_mode)) {
                    return LINGLONG_ERR("received fd is not a character device");
                }
            }
        }

        closeFd(listenFd);
        if (masterIn == -1) {
            return LINGLONG_ERR("oci runtime exited before sending console fd");
        }

        masterOut = ::dup(masterIn);
        if (masterOut < 0) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }
    } else {
        closeFd(nonTtyPipes->stdinPipe[0]);
        closeFd(nonTtyPipes->stdoutPipe[1]);
        closeFd(nonTtyPipes->stderrPipe[1]);

        masterOut = nonTtyPipes->stdinPipe[1];
        nonTtyPipes->stdinPipe[1] = -1;
        stdoutIn = nonTtyPipes->stdoutPipe[0];
        nonTtyPipes->stdoutPipe[0] = -1;
        stderrIn = nonTtyPipes->stderrPipe[0];
        nonTtyPipes->stderrPipe[0] = -1;
    }

    std::optional<struct termios> originalTermios;
    auto recoveryTermios = utils::finally::finally([&originalTermios]() {
        if (originalTermios) {
            auto ret = ::tcsetattr(STDOUT_FILENO, TCSAFLUSH, &originalTermios.value());
            if (ret != 0) {
                LogE("failed to set terminal attributes: {}", common::error::errorString(errno));
            }
        }
    });

    if (option.tty) {
        originalTermios.emplace();
        auto ret = ::tcgetattr(STDOUT_FILENO, &originalTermios.value());
        if (ret != 0) {
            return LINGLONG_ERR(fmt::format("failed to get terminal attributes: {}",
                                            common::error::errorString(errno)));
        }

        auto raw = originalTermios.value();
        cfmakeraw(&raw);

        if (tcsetattr(STDOUT_FILENO, TCSAFLUSH, &raw) < 0) {
            return LINGLONG_ERR(
              fmt::format("failed to set terminal to raw mode", common::error::errorString(errno)));
        }

        struct winsize ws{ };
        ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
        if (ret != 0) {
            return LINGLONG_ERR(
              fmt::format("failed to get terminal size: {}", common::error::errorString(errno)));
        }

        ret = ioctl(masterIn, TIOCSWINSZ, &ws);
        if (ret != 0) {
            return LINGLONG_ERR(fmt::format("failed to set terminal size to {}: {}",
                                            masterIn,
                                            common::error::errorString(errno)));
        }
    }

    auto closeMasterIn = utils::finally::finally([&masterIn]() {
        if (masterIn != -1) {
            ::close(masterIn);
        }
    });

    auto closeMasterOut = utils::finally::finally([&masterOut]() {
        if (masterOut != -1) {
            ::close(masterOut);
        }
    });

    auto closeStdoutIn = utils::finally::finally([&stdoutIn]() {
        if (stdoutIn != -1) {
            ::close(stdoutIn);
        }
    });

    auto closeStderrIn = utils::finally::finally([&stderrIn]() {
        if (stderrIn != -1) {
            ::close(stderrIn);
        }
    });

    std::optional<int> originalStdinFlags;
    auto nbRet = setNonBlock(STDIN_FILENO);
    if (!nbRet) {
        return LINGLONG_ERR(nbRet);
    }
    originalStdinFlags = *nbRet;

    auto restoreStdinFlags = utils::finally::finally([&originalStdinFlags]() {
        if (originalStdinFlags) {
            ::fcntl(STDIN_FILENO, F_SETFL, *originalStdinFlags);
        }
    });

    if (auto ret = setNonBlock(masterOut); !ret) {
        return LINGLONG_ERR(ret);
    }

    if (option.tty) {
        if (auto ret = setNonBlock(masterIn); !ret) {
            return LINGLONG_ERR(ret);
        }
    } else {
        if (auto ret = setNonBlock(stdoutIn); !ret) {
            return LINGLONG_ERR(ret);
        }
        if (auto ret = setNonBlock(stderrIn); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    auto epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        return LINGLONG_ERR(common::error::errorString(errno));
    }
    auto closeEpfd = utils::finally::finally([epfd]() {
        ::close(epfd);
    });

    auto addFd = [epfd](int fd, uint32_t events) -> utils::error::Result<void> {
        LINGLONG_TRACE(fmt::format("add fd {} to epoll", fd));
        struct epoll_event ev{ };
        ev.events = events;
        ev.data.fd = fd;
        if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }

        return LINGLONG_OK;
    };
    auto delFd = [epfd](int fd) {
        ::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    };

    if (option.tty) {
        if (auto ret = addFd(masterIn, EPOLLIN); !ret) {
            return LINGLONG_ERR(ret);
        }
    } else {
        if (auto ret = addFd(stdoutIn, EPOLLIN); !ret) {
            return LINGLONG_ERR(ret);
        }
        if (auto ret = addFd(stderrIn, EPOLLIN); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    if (auto ret = addFd(sfd, EPOLLIN); !ret) {
        return LINGLONG_ERR(ret);
    }

    bool stdinWatchable{ true };
    struct epoll_event stdinEv{ };
    stdinEv.events = EPOLLIN;
    stdinEv.data.fd = STDIN_FILENO;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &stdinEv) == -1) {
        if (errno != EPERM) {
            return LINGLONG_ERR(common::error::errorString(errno));
        }
        stdinWatchable = false;
    }

    std::vector<char> buffer(4096);
    std::array<struct epoll_event, 6> events{ };
    bool stdinClosed = false;
    bool inputWriteWatched = false;
    bool outputClosed = false;
    bool stdoutClosed = option.tty;
    bool stderrClosed = option.tty;
    std::string inputPending;

    auto writeAll = [](int fd, const char *data, ssize_t size) -> utils::error::Result<void> {
        LINGLONG_TRACE(fmt::format("write {} bytes to fd {}", size, fd));
        ssize_t written = 0;
        while (written < size) {
            auto w = ::write(fd, data + written, static_cast<size_t>(size - written));
            if (w == -1) {
                if (errno == EINTR) {
                    continue;
                }
                return LINGLONG_ERR(common::error::errorString(errno));
            }
            if (w == 0) {
                return LINGLONG_ERR("write returned 0");
            }
            written += w;
        }
        return LINGLONG_OK;
    };

    auto flushInput = [&]() -> utils::error::Result<void> {
        while (!inputPending.empty() && masterOut != -1) {
            auto w = ::write(masterOut, inputPending.data(), inputPending.size());
            if (w == -1) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (!inputWriteWatched) {
                        if (auto ret = addFd(masterOut, EPOLLOUT); !ret) {
                            return LINGLONG_ERR(ret);
                        }
                        inputWriteWatched = true;
                    }
                    return LINGLONG_OK;
                }
                if (errno != EPIPE) {
                    LogE("failed to write to container stdin: {}",
                         common::error::errorString(errno));
                }
                delFd(masterOut);
                closeFd(masterOut);
                inputPending.clear();
                return LINGLONG_OK;
            }
            if (w == 0) {
                return LINGLONG_ERR("write to container stdin returned 0");
            }
            inputPending.erase(0, static_cast<size_t>(w));
        }

        if (inputWriteWatched && inputPending.empty()) {
            delFd(masterOut);
            inputWriteWatched = false;
        }

        if (stdinClosed && inputPending.empty()) {
            closeFd(masterOut);
        }

        return LINGLONG_OK;
    };

    auto closeReadFd = [&](int &fd, bool &closed) {
        if (fd != -1) {
            delFd(fd);
            closeFd(fd);
        }

        closed = true;
    };

    auto readToOutput = [&](int &from, int to, bool &closed) -> utils::error::Result<void> {
        while (true) {
            auto n = ::read(from, buffer.data(), buffer.size());
            if (n > 0) {
                if (auto ret = writeAll(to, buffer.data(), n); !ret) {
                    LogE("failed to write output: {}", ret.error());
                    return LINGLONG_OK;
                }
                continue;
            }

            if (n == 0 || (n == -1 && option.tty && errno == EIO)) {
                LogD("output fd {} closed by peer", from);
                closeReadFd(from, closed);
            } else if (n == -1) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                LogE("failed to read container output: {}", common::error::errorString(errno));
                closeReadFd(from, closed);
            }
            break;
        }
        return LINGLONG_OK;
    };

    auto handleStdin = [&]() -> utils::error::Result<void> {
        while (true) {
            auto n = ::read(STDIN_FILENO, buffer.data(), buffer.size());
            if (n > 0) {
                inputPending.append(buffer.data(), static_cast<size_t>(n));
                if (auto ret = flushInput(); !ret) {
                    return LINGLONG_ERR(ret);
                }
                if (!inputPending.empty()) {
                    break;
                }
                continue;
            }

            if (n == 0) {
                delFd(STDIN_FILENO);
                stdinClosed = true;
                return flushInput();
            }
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            LogE("failed to read from stdin: {}", common::error::errorString(errno));
            delFd(STDIN_FILENO);
            stdinClosed = true;
            return flushInput();
        }
        return LINGLONG_OK;
    };

    if (!stdinWatchable) {
        if (auto ret = handleStdin(); !ret) {
            return LINGLONG_ERR(ret);
        }
    }

    while (!childExited || !(option.tty ? outputClosed : (stdoutClosed && stderrClosed))) {
        reapChild();
        auto nfds = ::epoll_wait(epfd, events.data(), events.size(), -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            return LINGLONG_ERR(
              fmt::format("epoll_wait error: {}", common::error::errorString(errno)));
        }

        for (int i = 0; i < nfds; ++i) {
            auto fd = events[i].data.fd;
            if (fd == sfd) {
                drainSignals();
            } else if (fd == STDIN_FILENO && !stdinClosed) {
                LogD("received input from stdin");
                if (auto ret = handleStdin(); !ret) {
                    return LINGLONG_ERR(ret);
                }
            } else if (fd == masterOut) {
                LogD("received output from container");
                if (auto ret = flushInput(); !ret) {
                    return LINGLONG_ERR(ret);
                }
                if (!stdinWatchable && !stdinClosed && inputPending.empty()) {
                    if (auto ret = handleStdin(); !ret) {
                        return LINGLONG_ERR(ret);
                    }
                }
            } else if (option.tty && fd == masterIn) {
                LogD("received input from container");
                if (auto ret = readToOutput(masterIn, STDOUT_FILENO, outputClosed); !ret) {
                    return LINGLONG_ERR(ret);
                }

                if (outputClosed) {
                    LogD("container output closed");
                }
            } else if (!option.tty && fd == stdoutIn) {
                LogD("received output from stdout");
                if (auto ret = readToOutput(stdoutIn, STDOUT_FILENO, stdoutClosed); !ret) {
                    return LINGLONG_ERR(ret);
                }
                if (stdoutClosed) {
                    LogD("container stdout closed");
                }
            } else if (!option.tty && fd == stderrIn) {
                LogD("received output from stderr");
                if (auto ret = readToOutput(stderrIn, STDERR_FILENO, stderrClosed); !ret) {
                    return LINGLONG_ERR(ret);
                }
                if (stderrClosed) {
                    LogD("container stderr closed");
                }
            }
        }
    }

    return reused ? 1 : -1;
}

int Cli::run(const RunOptions &options)
{
    LINGLONG_TRACE("command run");

    bool nvidiaCdiFound =
      std::any_of(options.cdiDevices.begin(), options.cdiDevices.end(), [](const std::string &d) {
          return d.find("nvidia.com/gpu") != std::string::npos;
      });
    std::optional<std::vector<api::types::v1::CdiDeviceEntry>> autoDetectedCdiDevices;

    if (!nvidiaCdiFound && options.cdiDevices.empty()) {
        auto allCdiDevices = cdi::getCDIDevices(options.cdiSpecDir, std::nullopt);
        if (allCdiDevices) {
            for (const auto &device : *allCdiDevices) {
                LogD("{}={} detected", device.kind, device.name);
                if (device.kind == "nvidia.com/gpu" && device.name == "all") {
                    nvidiaCdiFound = true;
                    autoDetectedCdiDevices = std::vector<api::types::v1::CdiDeviceEntry>{ device };
                    break;
                }
            }
        }
    }

    if (!nvidiaCdiFound) {
        detectDrivers();
    }

    auto userContainerDir = std::filesystem::path{ "/run/linglong" } / std::to_string(getuid());
    if (auto ret = utils::ensureDirectory(userContainerDir); !ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto curAppRef = this->repository.clearReference(*fuzzyRef,
                                                     {
                                                       .forceRemote = false,
                                                       .fallbackToRemote = false,
                                                     });
    if (!curAppRef) {
        this->printer.printErr(curAppRef.error());
        return -1;
    }

    auto loaded = linglong::utils::loadRuntimeConfig(options.appid);
    if (!loaded) {
        this->printer.printErr(loaded.error());
        return -1;
    }
    auto runtimeConfig = std::move(loaded).value();

    runtime::RunContext runContext(this->repository);
    linglong::runtime::ResolveOptions opts;
    opts.baseRef = options.base;
    opts.runtimeRef = options.runtime;
    // 处理多个扩展
    if (!options.extensions.empty()) {
        opts.extensionRefs = options.extensions;
    }
    if (runtimeConfig && runtimeConfig->extDefs) {
        opts.externalExtensionDefs = std::move(runtimeConfig->extDefs).value();
    }
    if (!options.cdiDevices.empty()) {
        auto cdiDevices = cdi::getCDIDevices(options.cdiSpecDir, options.cdiDevices);
        if (!cdiDevices) {
            handleCommonError(cdiDevices.error());
            return -1;
        }
        opts.cdiDevices = std::move(*cdiDevices);
    } else if (autoDetectedCdiDevices) {
        opts.cdiDevices = std::move(*autoDetectedCdiDevices);
    }

    // 调整日志输出，打印扩展列表（用逗号拼接）
    std::string extStr =
      opts.extensionRefs ? linglong::common::strings::join(*opts.extensionRefs, ',') : "null";
    LogD("start resolve run context with base {}, runtime {}, extensions {}",
         opts.baseRef.value_or("null"),
         opts.runtimeRef.value_or("null"),
         extStr);

    auto res = runContext.resolve(*curAppRef, opts);
    if (!res) {
        handleCommonError(res.error());
        return -1;
    }

    auto runContextCfg = runContext.getConfig();
    LogD("RunContext Config:\n{}", nlohmann::json(runContextCfg).dump());

    auto containerID = runContext.getContainerId();
    LogD("run app: {} container id: {}", curAppRef->toString(), containerID);

    const auto &appLayerItem = runContext.getCachedAppItem();
    if (!appLayerItem) {
        return -1;
    }
    const auto &info = appLayerItem->info;

    auto commands = options.commands;
    if (options.commands.empty()) {
        commands = info.command.value_or(std::vector<std::string>{ "bash" });
    }
    commands = filePathMapping(commands, options);

    const auto lockName = fmt::format(".cli.{}.lock", containerID);
    auto cliLockRet = utils::filelock::FileLock::create(userContainerDir / lockName,
                                                        utils::filelock::LockType::Write);
    if (!cliLockRet) {
        this->printer.printErr(cliLockRet.error());
        return -1;
    }
    auto cli_lock = std::move(cliLockRet).value();

    while (true) {
        auto lock_ret = cli_lock.tryLock(utils::filelock::LockType::Write);
        if (!lock_ret) {
            this->printer.printErr(lock_ret.error());
            return -1;
        }

        if (*lock_ret) {
            break;
        }

        auto ret = reuseContainer(containerID, commands);
        if (!ret) {
            LogW("unexpected reuse error: {}", ret.error());
            return -1;
        }

        if (*ret > 0) {
            LogD("container reuse succeeded");
            return 0;
        }

        if (*ret < 0) {
            LogW("failed to reuse container, try to start a new container", *ret);
            return -1;
        }

        // reuse failed, try again
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    LogD("start a new container");
    // this lambda will dump reference of containerID, app, base and runtime to
    // /run/linglong/<uid>/<pid> to store these needed information
    auto dumpContainerInfo = [&userContainerDir, &runContext, this]() -> bool {
        LINGLONG_TRACE("dump info")
        auto repoLock = utils::filelock::FileLock::create(common::dir::repoLockPath,
                                                          utils::filelock::LockType::Read,
                                                          false);
        if (!repoLock) {
            LogD("failed to create repo lock");
            this->printer.printErr(repoLock.error());
            return false;
        }
        auto lock = std::move(repoLock).value();
        auto ret = lock.lock(utils::filelock::LockType::Read);
        if (!ret) {
            LogD("failed to lock repo");
            this->printer.printErr(ret.error());
            return false;
        }

        auto pidFile = userContainerDir / std::to_string(getpid());
        std::ofstream stream{ pidFile };
        if (!stream.is_open()) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("failed to open {}", pidFile.c_str())));
            return false;
        }
        stream << nlohmann::json(runContext.stateInfo());
        stream.close();

        return true;
    };

    if (!dumpContainerInfo()) {
        return -1;
    }

    auto cacheRes = this->ensureCache(runContext);
    if (!cacheRes) {
        this->printer.printErr(LINGLONG_ERRV(cacheRes));
        return -1;
    }

    auto namespaceRes = linglong::utils::needRunInNamespace();
    if (!namespaceRes) {
        this->printer.printErr(namespaceRes.error());
        return -1;
    }

    if (*namespaceRes) {
        LogD("run in new namespace");
        const auto qtArgs = QCoreApplication::arguments();
        auto selfExe = linglong::utils::getSelfExe();
        if (!selfExe) {
            this->printer.printErr(selfExe.error());
            return -1;
        }

        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(qtArgs.size()) + 2);
        for (const auto &arg : qtArgs) {
            args.emplace_back(arg.toStdString());
        }
        args[0] = std::move(*selfExe);

        auto insertPos = std::find(args.begin(), args.end(), std::string{ "run" });
        if (insertPos == args.end()) {
            this->printer.printErr(LINGLONG_ERRV("failed to locate run subcommand"));
            return -1;
        }

        auto contextJson = nlohmann::json(runContext.getConfig()).dump();
        args.insert(insertPos + 1, { "--run-context", contextJson });

        std::vector<char *> argPointers;
        argPointers.reserve(args.size() + 1);
        for (auto &arg : args) {
            argPointers.push_back(arg.data());
        }
        argPointers.push_back(nullptr);

        auto runRes = linglong::utils::runInNamespace(static_cast<int>(args.size()),
                                                      argPointers.data(),
                                                      geteuid(),
                                                      getegid());
        if (!runRes) {
            this->printer.printErr(runRes.error());
            return -1;
        }

        return *runRes;
    }

    return this->runResolvedContext(runContext, options, std::move(runtimeConfig));
}

int Cli::runWithContext(const RunOptions &options)
{
    LINGLONG_TRACE("command run with context");

    if (!options.runContext) {
        this->printer.printErr(LINGLONG_ERRV("run context is required"));
        return -1;
    }

    auto loaded = linglong::utils::loadRuntimeConfig(options.appid);
    if (!loaded) {
        this->printer.printErr(loaded.error());
        return -1;
    }
    auto runtimeConfig = std::move(loaded).value();

    runtime::RunContext runContext(this->repository);
    try {
        auto cfg =
          nlohmann::json::parse(*options.runContext).get<api::types::v1::RunContextConfig>();
        auto res = runContext.resolve(cfg);
        if (!res) {
            this->printer.printErr(res.error());
            return -1;
        }
    } catch (const std::exception &e) {
        this->printer.printErr(
          LINGLONG_ERRV(fmt::format("failed to parse run context: {}", e.what())));
        return -1;
    }

    return this->runResolvedContext(runContext, options, std::move(runtimeConfig));
}

int Cli::runResolvedContext(runtime::RunContext &runContext,
                            const RunOptions &options,
                            std::optional<api::types::v1::RuntimeConfigure> runtimeConfig)
{
    LINGLONG_TRACE("run resolved context");

    auto appLayerItem = runContext.getCachedAppItem();
    if (!appLayerItem) {
        this->printer.printErr(LINGLONG_ERRV("failed to get cached app item"));
        return -1;
    }

    auto commands = options.commands;
    if (options.commands.empty()) {
        commands = appLayerItem->info.command.value_or(std::vector<std::string>{ "bash" });
    }
    commands = filePathMapping(commands, options);

    auto appCache =
      common::dir::getContainerCacheDir(appLayerItem->commit, runContext.getContainerId());

    runtime::RunContainerOptions runOptions;
    runOptions.enableSecurityContext(runtime::getDefaultSecurityContexts());
    runOptions.common.containerCachePath = appCache;
    runOptions.lockName = ".lock";
    if (runtimeConfig) {
        auto runtimeConfigRes = runOptions.applyRuntimeConfig(*runtimeConfig);
        if (!runtimeConfigRes) {
            this->printer.printErr(runtimeConfigRes.error());
            return -1;
        }
    }
    auto res = runOptions.applyCliRunOptions(options);
    if (!res) {
        this->printer.printErr(res.error());
        return -1;
    }

    const auto &appid = runContext.getTargetID();
    if (!options.disableXdp.has_value() && !utils::isValidXdgDesktopPortalId(appid)) {
        LogW("appid '{}' doesn't conform to XDP ID specification, disabling XDP integration. "
             "Use --enable-xdp to override.",
             appid);
        runOptions.disableXdp = true;
    }

    auto containerRet = this->containerBuilder.createRunContainer(runContext, runOptions);
    if (!containerRet) {
        this->printer.printErr(containerRet.error());
        return -1;
    }
    auto container = std::move(containerRet).value();

    auto containerLock =
      utils::filelock::FileLock::create(container->bundle() / runOptions.lockName,
                                        utils::filelock::LockType::Write);
    if (!containerLock) {
        this->printer.printErr(containerLock.error());
        return -1;
    }
    auto lock = std::move(containerLock).value();
    auto lockRet = lock.tryLock(utils::filelock::LockType::Write);
    if (!lockRet) {
        this->printer.printErr(lockRet.error());
        return -1;
    }

    if (!*lockRet) {
        this->printer.printErr(LINGLONG_ERRV("failed to acquire container lock"));
        return -1;
    }

    auto fd = lock.nativeHandle();
    const auto &content = "initializing";
    if (::write(fd, static_cast<const void *>(content), sizeof(content) - 1) == -1) {
        this->printer.printErr(LINGLONG_ERRV(
          fmt::format("failed to write to lock file: {}", common::error::errorString(errno))));
        return -1;
    }

    auto ret = lock.unlock();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto process = ocppi::runtime::config::types::Process{ .args = std::move(commands) };
    if (!options.workdir.value_or("").empty()) {
        auto workdir = std::filesystem::path(options.workdir.value());
        if (!workdir.is_absolute()) {
            auto msg = fmt::format("Workdir must be an absolute path: {}", workdir);
            this->printer.printErr(LINGLONG_ERRV(msg));
            return -1;
        }
        process.cwd = workdir;
    }

    auto result = container->run(process, { });
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    return 0;
}

int Cli::enter(const EnterOptions &options)
{
    LINGLONG_TRACE("ll-cli exec");
    auto containerIDList = this->getRunningAppContainers(options.instance);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    auto containerID = containerIDList->front();
    LogI("select container id: {}", containerID);
    auto commands = options.commands;
    if (commands.empty()) {
        commands = utils::BashCommandHelper::generateDefaultBashCommand();
    }

    auto opt = ocppi::runtime::ExecOption{
        .uid = ::getuid(),
        .gid = ::getgid(),
    };

    auto result =
      this->ociCLI.exec(containerID, commands[0], { commands.begin() + 1, commands.end() }, opt);
    if (!result) {
        auto err = LINGLONG_ERRV(result);
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

utils::error::Result<std::vector<api::types::v1::CliContainer>>
Cli::getCurrentContainers() const noexcept
{
    LINGLONG_TRACE("get current running containers")

    auto containersRet = this->ociCLI.list();
    if (!containersRet) {
        return LINGLONG_ERR(containersRet);
    }
    auto containers = std::move(containersRet).value();

    std::vector<api::types::v1::CliContainer> myContainers;
    auto infoDir = std::filesystem::path{ "/run/linglong" } / std::to_string(::getuid());

    std::error_code ec;
    auto it = std::filesystem::directory_iterator{ infoDir, ec };
    if (ec) {
        if (ec == std::errc::no_such_file_or_directory) {
            return myContainers;
        }

        return LINGLONG_ERR(fmt::format("failed to list {}", infoDir), ec);
    }

    for (const auto &pidFile : it) {
        const auto &file = pidFile.path();
        const auto &process = "/proc" / file.filename();

        std::error_code ec;
        if (!std::filesystem::exists(process, ec)) {
            // this process may exit abnormally, skip it.
            LogD("{} doesn't exist", process.string());
            continue;
        }

        if (pidFile.file_size(ec) == 0) {
            continue;
        }

        auto info = linglong::utils::serialize::LoadJSONFile<
          linglong::api::types::v1::ContainerProcessStateInfo>(file);
        if (!info) {
            LogD("load info from {}: {}", file.string(), info.error());
            continue;
        }

        auto container = std::find_if(containers.begin(),
                                      containers.end(),
                                      [&info](const ocppi::types::ContainerListItem &item) {
                                          return item.id == info->containerID;
                                      });
        if (container == containers.cend()) {
            LogD("couldn't find container that process {} belongs to", file.filename().string());
            continue;
        }

        myContainers.emplace_back(api::types::v1::CliContainer{
          .id = std::move(info->containerID),
          .package = std::move(info->app),
          .pid = container->pid,
        });
    }

    return myContainers;
}

int Cli::ps()
{
    auto myContainers = getCurrentContainers();
    if (!myContainers) {
        this->printer.printErr(myContainers.error());
        return -1;
    }

    // TODO: add option --no-truncated
    std::for_each(myContainers->begin(),
                  myContainers->end(),
                  [](api::types::v1::CliContainer &container) {
                      container.id = container.id.substr(0, ContainerIDDisplayLength);
                  });

    this->printer.printContainers(*myContainers);

    return 0;
}

bool Cli::isContainerIDMatch(const std::string &containerID, const std::string &shortID)
{
    if (containerID == shortID) {
        return true;
    }

    if (shortID.size() < ContainerIDDisplayLength) {
        return false;
    }

    return common::strings::starts_with(containerID, shortID);
}

utils::error::Result<std::vector<std::string>> Cli::getRunningAppContainers(const std::string &id)
{
    LINGLONG_TRACE("get app running containers");

    std::vector<std::string> containerIDList{ };
    auto containers = getCurrentContainers();
    if (!containers) {
        return LINGLONG_ERR(containers);
    }

    for (const auto &container : *containers) {
        // first check if the id matches container id, then check if the id matches package appid or
        // reference
        if (isContainerIDMatch(container.id, id)) {
            containerIDList.emplace_back(container.id);
            continue;
        }

        auto ref = package::Reference::parse(container.package);
        if (!ref) {
            LogW("{}", ref.error());
            continue;
        }

        if (ref->id == id || ref->toString() == id) {
            containerIDList.emplace_back(container.id);
        }
    }

    if (containerIDList.size() > 1) {
        auto msg =
          fmt::format("multiple running containers match the specified identifier '{}': {}. "
                      "Please specify a more specific identifier.",
                      id,
                      containerIDList);
        return LINGLONG_ERR(msg);
    }

    return containerIDList;
}

int Cli::kill(const KillOptions &options)
{
    LINGLONG_TRACE("command kill");

    auto containerIDList = this->getRunningAppContainers(options.appid);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("no container found"));
        return -1;
    }

    auto ret = 0;
    for (const auto &containerID : *containerIDList) {
        LogI("select container id {}", containerID);
        auto result = this->ociCLI.kill(ocppi::runtime::ContainerID(containerID),
                                        ocppi::runtime::Signal(options.signal));
        if (!result) {
            auto err = LINGLONG_ERRV(result);
            this->printer.printErr(err);
            ret = -1;
        }
    }

    return ret;
}

void Cli::cancelCurrentTask()
{
    if (this->task != nullptr) {
        LogD("cancel running task");
        this->task->Cancel();
    }
}

int Cli::installFromFile(const QFileInfo &fileInfo,
                         const api::types::v1::CommonOptions &commonOptions,
                         const std::string &appid)
{
    auto filePath = fileInfo.absoluteFilePath();
    LINGLONG_TRACE(fmt::format("install from file {}", filePath.toStdString()));

    auto authReply = this->authorization();
    if (!authReply.isValid()) {
        if (authReply.error().type() == QDBusError::AccessDenied) {
            auto args = QCoreApplication::instance()->arguments();
            // pkexec在0.120版本之前没有keep-cwd选项，会将目录切换到/root
            // 所以将layer或uab文件的相对路径转为绝对路径，再传给pkexec
            auto path = fileInfo.absoluteFilePath();
            for (auto i = 0; i < args.length(); i++) {
                if (args[i] == QString::fromStdString(appid)) {
                    args[i] = path.toLocal8Bit().constData();
                }
            }

            auto ret = this->runningAsRoot(args);
            if (!ret) {
                this->printer.printErr(ret.error());
            }
            return -1;
        }

        this->printer.printErr(LINGLONG_ERRV(
          fmt::format("{} {}", authReply.error().message() + authReply.error().name()),
          static_cast<int>(authReply.error().type())));
        return -1;
    }

    auto res = this->initInteraction();
    if (!res) {
        this->printer.printErr(res.error());
        return -1;
    }

    LogI("install from file {}", filePath.toStdString());
    QFile file{ filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::ExistingOnly)) {
        auto err = LINGLONG_ERR(file.errorString().toStdString());
        this->printer.printErr(err.value());
        return -1;
    }

    QDBusUnixFileDescriptor dbusFileDescriptor(file.handle());

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->InstallFromFile(dbusFileDescriptor,
                                                   fileInfo.suffix(),
                                                   common::serialize::toQVariantMap(commonOptions));
    res = waitTaskCreated(pendingReply, TaskType::InstallFromFile);
    if (!res) {
        this->handleCommonError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::install(const InstallOptions &options)
{
    LINGLONG_TRACE("command install");

    auto params =
      api::types::v1::PackageManager1InstallParameters{ .options = { .force = false,
                                                                     .skipInteraction = false } };
    params.options.force = options.forceOpt;
    params.options.skipInteraction = options.confirmOpt;
    params.repo = options.repo;

    QFileInfo info(QString::fromStdString(options.appid));

    // 如果检测是文件，则直接安装
    if (info.exists() && info.isFile()) {
        return installFromFile(QFileInfo{ info.absoluteFilePath() }, params.options, options.appid);
    }

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    ret = this->initInteraction();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }

    if (!options.module.empty()) {
        params.package.modules = { options.module };
    } else {
        params.package.modules = getAutoModuleList();
    }

    LogD("install module: {}", common::strings::join(*params.package.modules));

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Install(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Install);
    if (!res) {
        handleInstallError(res.error(), params);
        return -1;
    }
    this->taskState.params = std::move(params);

    waitTaskDone();

    updateAM();
    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::upgrade(const UpgradeOptions &options)
{
    LINGLONG_TRACE("command upgrade");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    std::vector<package::Reference> toUpgrade;
    if (!options.appid.empty()) {
        auto fuzzyRef = package::FuzzyReference::parse(options.appid);
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto localRef = this->repository.clearReference(*fuzzyRef,
                                                        {
                                                          .forceRemote = false,
                                                          .fallbackToRemote = false,
                                                        });
        if (!localRef) {
            this->printer.printMessage(
              fmt::format(_("Application {} is not installed."), options.appid));
            return -1;
        }

        auto layerItemRet = this->repository.getLayerItem(*localRef);
        if (!layerItemRet) {
            this->printer.printErr(layerItemRet.error());
            return -1;
        }
        if (layerItemRet->info.kind != "app") {
            this->printer.printMessage(fmt::format(_("{} is not an application."), options.appid));
            return -1;
        }
        toUpgrade.emplace_back(std::move(localRef).value());
    }

    api::types::v1::PackageManager1UpdateParameters params;
    params.appOnly = options.appOnly;
    params.depsOnly = options.depsOnly;
    for (const auto &ref : toUpgrade) {
        api::types::v1::PackageManager1Package package;
        package.id = ref.id;
        package.channel = ref.channel;
        params.packages.emplace_back(std::move(package));
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Update(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Upgrade);
    if (!res) {
        handleUpgradeError(res.error());
        return -1;
    }

    waitTaskDone();

    updateAM();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::search(const SearchOptions &options)
{
    LINGLONG_TRACE("command search");

    auto params = api::types::v1::PackageManager1SearchParameters{
        .id = options.appid,
        .repos = { },
    };

    auto repoConfig = this->repository.getOrderedConfig();
    if (repoConfig.repos.empty()) {
        this->printer.printErr(LINGLONG_ERRV("no repo found"));
        return -1;
    }

    if (options.repo) {
        auto it = std::find_if(repoConfig.repos.begin(),
                               repoConfig.repos.end(),
                               [&options](const api::types::v1::Repo &repo) {
                                   return repo.alias.value_or(repo.name) == options.repo.value();
                               });
        if (it == repoConfig.repos.end()) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("repo {} not found", options.repo.value())));
            return -1;
        }
        params.repos.emplace_back(options.repo.value());
    } else {
        // search all repos
        for (const auto &repo : repoConfig.repos) {
            params.repos.emplace_back(repo.alias.value_or(repo.name));
        }
    }

    std::optional<QString> pendingJobID;

    QEventLoop loop;
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    connect(
      *pkgMan,
      &api::dbus::v1::PackageManager::SearchFinished,
      [&pendingJobID, this, &loop, &options](const QString &jobID, const QVariantMap &data) {
          LINGLONG_TRACE("process search result");
          // Note: once an error occurs, remember to return after exiting the loop.
          if (!pendingJobID || *pendingJobID != jobID) {
              return;
          }
          auto result =
            common::serialize::fromQVariantMap<api::types::v1::PackageManager1SearchResult>(data);
          if (!result) {
              this->printer.printErr(result.error());
              loop.exit(-1);
              return;
          }
          // Note: should check return code of PackageManager1SearchResult
          auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
          if (resultCode != utils::error::ErrorCode::Success) {
              if (resultCode == utils::error::ErrorCode::Failed) {
                  this->printer.printErr(LINGLONG_ERRV("\n" + result->message, result->code));
                  loop.exit(result->code);
                  return;
              }

              if (resultCode == utils::error::ErrorCode::NetworkError) {
                  this->printer.printMessage(_("Network connection failed. Please:"
                                               "\n1. Check your internet connection"
                                               "\n2. Verify network proxy settings if used"));
              }

              if (this->globalOptions.verbose) {
                  this->printer.printErr(LINGLONG_ERRV("\n" + result->message, result->code));
              }

              loop.exit(result->code);
              return;
          }

          if (!result->packages) {
              this->printer.printPackages({ });
              loop.exit(0);
              return;
          }

          auto allPackages = std::move(result->packages).value();
          if (!options.showDevel) {
              std::for_each(allPackages.begin(),
                            allPackages.end(),
                            [](decltype(allPackages)::reference pkgs) {
                                auto &vec = pkgs.second;

                                auto it =
                                  std::remove_if(vec.begin(),
                                                 vec.end(),
                                                 [](const api::types::v1::PackageInfoV2 &pkg) {
                                                     return pkg.packageInfoV2Module == "develop";
                                                 });
                                vec.erase(it, vec.end());
                            });
          }

          if (!options.type.empty()) {
              filterPackageInfosByType(allPackages, options.type);
          }

          // default only the latest version is displayed
          if (!options.showAllVersion) {
              filterPackageInfosByVersion(allPackages);
          }

          this->printer.printSearchResult(allPackages);
          loop.exit(0);
      });

    auto pendingReply = (*pkgMan)->Search(common::serialize::toQVariantMap(params));
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }

    pendingJobID = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::prune()
{
    LINGLONG_TRACE("command prune");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    QEventLoop loop;
    QString jobIDReply = "";
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    connect(*pkgMan,
            &api::dbus::v1::PackageManager::PruneFinished,
            [this, &loop, &jobIDReply](const QString &jobID, const QVariantMap &data) {
                LINGLONG_TRACE("process prune result");
                if (jobIDReply != jobID) {
                    return;
                }
                auto ret =
                  common::serialize::fromQVariantMap<api::types::v1::PackageManager1PruneResult>(
                    data);
                if (!ret) {
                    this->printer.printErr(ret.error());
                    loop.exit(-1);
                    return;
                }

                if (!ret->packages) {
                    this->printer.printErr(LINGLONG_ERRV("No packages to prune."));
                    loop.exit(0);
                    return;
                }

                this->printer.printPruneResult(*ret->packages);
                loop.exit(0);
            });

    auto pendingReply = (*pkgMan)->Prune();
    auto result = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!result) {
        this->printer.printErr(result.error());
        return -1;
    }
    jobIDReply = QString::fromStdString(result->id);

    return loop.exec();
}

int Cli::uninstall(const UninstallOptions &options)
{
    LINGLONG_TRACE("command uninstall");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto params = api::types::v1::PackageManager1UninstallParameters{ };
    params.options = api::types::v1::CommonOptions{
        .force = options.forceOpt,
        .skipInteraction = false,
    };
    params.package.id = fuzzyRef->id;
    if (fuzzyRef->channel) {
        params.package.channel = fuzzyRef->channel;
    }
    if (fuzzyRef->version) {
        params.package.version = fuzzyRef->version;
    }
    if (!options.module.empty()) {
        params.package.packageManager1PackageModule = options.module;
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto pendingReply = (*pkgMan)->Uninstall(common::serialize::toQVariantMap(params));
    auto res = waitTaskCreated(pendingReply, TaskType::Uninstall);
    if (!res) {
        this->handleUninstallError(res.error());
        return -1;
    }

    waitTaskDone();

    return this->taskState.state == linglong::api::types::v1::State::Succeed ? 0 : -1;
}

int Cli::list(const ListOptions &options)
{
    if (options.showUpgradeList) {
        auto upgradeList = this->listUpgradable();
        if (!upgradeList) {
            this->printer.printErr(upgradeList.error());
            return -1;
        }
        // 按id排序
        std::sort(upgradeList->begin(), upgradeList->end(), [](const auto &lhs, const auto &rhs) {
            return lhs.id < rhs.id;
        });
        this->printer.printUpgradeList(*upgradeList);
        return 0;
    }
    auto items = this->repository.listLayerItem();
    if (!items) {
        this->printer.printErr(items.error());
        return -1;
    }
    std::vector<api::types::v1::PackageInfoDisplay> list;
    for (const auto &item : *items) {
        nlohmann::json json = item.info;
        auto m = json.get<api::types::v1::PackageInfoDisplay>();
        auto t = this->repository.getLayerCreateTime(item);
        if (t.has_value()) {
            m.installTime = *t;
        }
        list.push_back(std::move(m));
    }
    if (!options.type.empty()) {
        filterPackageInfosByType(list, options.type);
    }
    // 按id排序
    std::sort(list.begin(), list.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.id < rhs.id;
    });
    this->printer.printPackages(list);
    return 0;
}

utils::error::Result<std::vector<api::types::v1::UpgradeListResult>> Cli::listUpgradable()
{
    LINGLONG_TRACE("list upgradable");

    // only applications can be upgraded
    auto upgradablePkgs = this->repository.upgradableApps();
    if (!upgradablePkgs) {
        return LINGLONG_ERR(upgradablePkgs);
    }

    std::vector<api::types::v1::UpgradeListResult> upgradeList;
    for (const auto &pkg : *upgradablePkgs) {
        upgradeList.emplace_back(
          api::types::v1::UpgradeListResult{ .id = pkg.first.id,
                                             .newVersion = pkg.second.reference.version.toString(),
                                             .oldVersion = pkg.first.version.toString() });
    }
    return upgradeList;
}

int Cli::repo(CLI::App *app, const RepoOptions &options)
{
    LINGLONG_TRACE("command repo");

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    auto propCfg = (*pkgMan)->configuration();
    if ((*pkgMan)->lastError().isValid()) {
        auto err = LINGLONG_ERRV((*pkgMan)->lastError().message().toStdString());
        this->printer.printErr(err);
        return -1;
    }

    auto cfg = common::serialize::fromQVariantMap<api::types::v1::RepoConfigV2>(propCfg);
    if (!cfg) {
        LogE("fatal error: {}", cfg.error());
        std::abort();
    }

    auto argsParsed = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParsed("show")) {
        this->printer.printRepoConfig(*cfg);
        return 0;
    }

    if (argsParsed("modify")) {
        this->printer.printErr(
          LINGLONG_ERRV("sub-command 'modify' already has been deprecated, please use sub-command "
                        "'add' to add a remote repository and use it as default."));
        return EINVAL;
    }

    std::string url = options.repoUrl;

    if (argsParsed("add") || argsParsed("update")) {
        if (url.rfind("http", 0) != 0) {
            this->printer.printErr(LINGLONG_ERRV(fmt::format("url is invalid: {}", url)));
            return EINVAL;
        }

        // remove last slash
        if (url.back() == '/') {
            url.pop_back();
        }
    }

    std::string name = options.repoName;
    // if alias is not set, use name as alias
    std::string alias = options.repoAlias.value_or(name);
    auto &cfgRef = *cfg;

    if (argsParsed("add")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return EINVAL;
        }

        bool isExist =
          std::any_of(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
              return repo.alias.value_or(repo.name) == alias;
          });
        if (isExist) {
            this->printer.printErr(LINGLONG_ERRV(fmt::format("repo {} already exist", alias)));
            return -1;
        }
        cfgRef.repos.push_back(api::types::v1::Repo{
          .alias = options.repoAlias,
          .name = name,
          .priority = 0,
          .url = url,
        });
        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    auto existingRepo =
      std::find_if(cfgRef.repos.begin(), cfgRef.repos.end(), [&alias](const auto &repo) {
          return repo.alias.value_or(repo.name) == alias;
      });

    if (existingRepo == cfgRef.repos.end()) {
        this->printer.printErr(
          LINGLONG_ERRV(fmt::format("the operated repo {} doesn't exist", name)));
        return -1;
    }

    if (argsParsed("remove")) {
        if (cfgRef.repos.size() == 1) {
            this->printer.printErr(
              LINGLONG_ERRV(fmt::format("repo {} is the only repo, please add another repo before "
                                        "removing it or update it directly.",
                                        alias)));
            return -1;
        }
        cfgRef.repos.erase(existingRepo);

        if (cfgRef.defaultRepo == alias) {
            // choose the max priority repo as default repo
            auto maxPriority = linglong::repo::getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.priority == maxPriority) {
                    cfgRef.defaultRepo = repo.alias.value_or(repo.name);
                    break;
                }
            }
        }

        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("update")) {
        if (url.empty()) {
            this->printer.printErr(LINGLONG_ERRV("url is empty."));
            return -1;
        }

        existingRepo->url = url;
        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("enable-mirror")) {
        existingRepo->mirrorEnabled = true;
        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("disable-mirror")) {
        existingRepo->mirrorEnabled = false;
        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    if (argsParsed("set-default")) {
        if (cfgRef.defaultRepo != alias) {
            cfgRef.defaultRepo = alias;
            // set-default is equal to set-priority to the current max priority + 100
            auto maxPriority = linglong::repo::getRepoMaxPriority(cfgRef);
            for (auto &repo : cfgRef.repos) {
                if (repo.alias.value_or(repo.name) == alias) {
                    repo.priority = maxPriority + 100;
                    break;
                }
            }
            return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
        }

        return 0;
    }

    if (argsParsed("set-priority")) {
        existingRepo->priority = options.repoPriority;
        return this->setRepoConfig(common::serialize::toQVariantMap(cfgRef));
    }

    this->printer.printErr(LINGLONG_ERRV("unknown operation"));
    return -1;
}

int Cli::setRepoConfig(const QVariantMap &config)
{
    LINGLONG_TRACE("set repo config");

    auto ret = this->ensureAuthorized();
    if (!ret) {
        this->printer.printErr(ret.error());
        return -1;
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        this->printer.printErr(pkgMan.error());
        return -1;
    }

    (*pkgMan)->setConfiguration(config);
    if ((*pkgMan)->lastError().isValid()) {
        auto err = LINGLONG_ERRV((*pkgMan)->lastError().message().toStdString());
        this->printer.printErr(err);
        return -1;
    }
    return 0;
}

int Cli::info(const InfoOptions &options)
{
    LINGLONG_TRACE("command info");

    QString app = QString::fromStdString(options.appid);

    if (app.isEmpty()) {
        auto err = LINGLONG_ERR("failed to get layer path").value();
        this->printer.printErr(err);
        return err.code();
    }

    QFileInfo file(app);
    auto isLayerFile = file.isFile() && file.suffix() == "layer";

    // 如果是app，显示app 包信息
    if (!isLayerFile) {
        auto fuzzyRef = package::FuzzyReference::parse(app.toStdString());
        if (!fuzzyRef) {
            this->printer.printErr(fuzzyRef.error());
            return -1;
        }

        auto ref =
          this->repository.clearReference(*fuzzyRef,
                                          { .forceRemote = false, .fallbackToRemote = false });
        if (!ref) {
            LogD("{}", ref.error());
            this->printer.printErr(LINGLONG_ERRV("Cannot find such application.",
                                                 utils::error::ErrorCode::AppNotFoundFromLocal));
            return -1;
        }

        auto layer = this->repository.getLayerDir(*ref, "binary");
        if (!layer) {
            this->printer.printErr(layer.error());
            return -1;
        }

        auto info = layer->info();
        if (!info) {
            this->printer.printErr(info.error());
            return -1;
        }
        this->printer.printPackage(*info);

        return 0;
    }

    // 如果是layer文件，显示layer文件 包信息
    const auto layerFile = package::LayerFile::New(app);

    if (!layerFile) {
        this->printer.printErr(layerFile.error());
        return -1;
    }

    const auto layerInfo = (*layerFile)->metaInfo();
    if (!layerInfo) {
        this->printer.printErr(layerInfo.error());
        return -1;
    }

    this->printer.printLayerInfo(*layerInfo);
    return 0;
}

int Cli::content(const ContentOptions &options)
{
    LINGLONG_TRACE("command content");

    QStringList contents{ };

    auto fuzzyRef = package::FuzzyReference::parse(options.appid);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               { .forceRemote = false, .fallbackToRemote = false });
    if (!ref) {
        LogD("{}", ref.error());
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    auto layerItem = this->repository.getLayerItem(*ref);
    if (!layerItem) {
        this->printer.printErr(layerItem.error());
        return -1;
    }

    if (layerItem->info.kind != "app") {
        this->printer.printErr(LINGLONG_ERRV("Only supports viewing app content"));
        return -1;
    }

    auto layer = this->repository.getLayerDir(*ref, "binary");
    if (!layer) {
        this->printer.printErr(layer.error());
        return -1;
    }

    QDir entriesDir((layer->path() / "entries").c_str());
    if (!entriesDir.exists()) {
        this->printer.printErr(LINGLONG_ERR("no entries found").value());
        return -1;
    }

    const auto preferLibSystemdUser = QFileInfo(entriesDir.filePath("lib/systemd/user")).exists();

    QDirIterator it(entriesDir.absolutePath(),
                    QDir::AllEntries | QDir::NoDot | QDir::NoDotDot | QDir::System,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        it.next();
        const auto entryPath = it.fileInfo().absoluteFilePath();
        const auto relativePath =
          std::filesystem::path(entriesDir.relativeFilePath(entryPath).toStdString());
        const auto exportPath =
          this->repository.resolveEntryExportPath(relativePath, preferLibSystemdUser);
        if (!exportPath.empty()) {
            contents.append(QString::fromStdString(exportPath.string()));
        }
    }

    // only show the contents which are exported
    QStringList exportedContents{ };
    for (const auto &content : std::as_const(contents)) {
        QFileInfo info(content);
        if (!info.exists() || info.isDir()) {
            continue;
        }
        exportedContents.append(content);
    }

    this->printer.printContent(exportedContents);
    return 0;
}

[[nodiscard]] std::string Cli::mappingFile(const std::filesystem::path &file) noexcept
{
    std::error_code ec;
    auto target = file;

    if (!target.is_absolute()) {
        return target;
    }

    if (std::filesystem::is_symlink(file, ec)) {
        std::array<char, PATH_MAX + 1> buf{ };
        auto *real = ::realpath(file.c_str(), buf.data());

        if (real != nullptr) {
            target = real;
        } else {
            LogE("resolve symlink {} error: {}", file, common::error::errorString(errno));
        }
    }

    if (ec) {
        LogE("failed to check symlink {}: {}", file.c_str(), ec.message());
    }

    // Dont't mapping the file under /home
    if (auto tmp = target.string(); tmp.rfind("/home/", 0) == 0) {
        return target;
    }

    return std::filesystem::path{ "/run/host/rootfs" }
    / std::filesystem::path{ target }.lexically_relative("/");
}

[[nodiscard]] std::string Cli::mappingUrl(std::string_view url) noexcept
{
    if (url.rfind('/', 0) == 0) {
        return mappingFile(url);
    }

    // if the scheme of url is "file", we need to map the native file path to the corresponding
    // container path, others will deliver to app directly.
    constexpr std::string_view filePrefix = "file://";
    if (url.rfind(filePrefix, 0) == 0) {
        std::filesystem::path nativePath = url.substr(filePrefix.size());
        std::filesystem::path target = mappingFile(nativePath);
        return std::string{ filePrefix } + target.string();
    }

    return std::string{ url };
}

std::vector<std::string> Cli::filePathMapping(const std::vector<std::string> &command,
                                              const RunOptions &options) const noexcept
{
    // FIXME: couldn't handel command like 'll-cli run org.xxx.yyy --file f1 f2 f3 org.xxx.yyy %%F'
    // can't distinguish the boundary of command , need validate the command arguments in the future

    std::vector<std::string> execArgs;
    // if the --file or --url option is specified, need to map the file path to the linglong
    // path(/run/host).
    for (const auto &arg : command) {
        if (arg.rfind('%', 0) != 0) {
            execArgs.emplace_back(arg);
            continue;
        }

        if (arg == "%f" || arg == "%F") {
            if (arg == "%f" && options.filePaths.size() > 1) {
                // refer:
                // https://specifications.freedesktop.org/desktop-entry-spec/latest/exec-variables.html
                LogW("more than one file path specified, all file paths will be passed.");
            }

            for (const auto &file : options.filePaths) {
                if (file.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingFile(file));
            }

            continue;
        }

        if (arg == "%u" || arg == "%U") {
            if (arg == "%u" && options.fileUrls.size() > 1) {
                LogW("more than one url specified, all file paths will be passed.");
            }

            for (const auto &url : options.fileUrls) {
                if (url.empty()) {
                    continue;
                }

                execArgs.emplace_back(mappingUrl(url));
            }

            continue;
        }

        LogW("unkown command argument {}", arg);
    }

    return execArgs;
}

void Cli::filterPackageInfosByType(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list,
  const std::string &type) noexcept
{
    // if type is all, do nothing, return app of all packages.
    if (type == "all") {
        return;
    }

    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> filtered;

    for (const auto &[key, packages] : list) {
        std::vector<api::types::v1::PackageInfoV2> filteredPackages;

        std::copy_if(packages.begin(),
                     packages.end(),
                     std::back_inserter(filteredPackages),
                     [&type](const api::types::v1::PackageInfoV2 &pkg) {
                         return pkg.kind == type;
                     });

        if (!filteredPackages.empty()) {
            filtered.emplace(key, std::move(filteredPackages));
        }
    }

    list = std::move(filtered);
}

void Cli::filterPackageInfosByType(std::vector<api::types::v1::PackageInfoDisplay> &list,
                                   const std::string &type)
{
    if (type == "all") {
        return;
    }
    list.erase(std::remove_if(list.begin(),
                              list.end(),
                              [&type](const api::types::v1::PackageInfoDisplay &pkg) {
                                  return pkg.kind != type;
                              }),
               list.end());
}

void Cli::filterPackageInfosByVersion(
  std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> &list) noexcept
{
    for (const auto &[pkgRepo, packages] : list) {
        std::map<std::string, api::types::v1::PackageInfoV2> temp;
        for (const auto &pkgInfo : packages) {
            auto key =
              QString("%1-%2-%3")
                .arg(pkgRepo.c_str(), pkgInfo.id.c_str(), pkgInfo.packageInfoV2Module.c_str())
                .toStdString();

            auto it = temp.find(key);
            if (it == temp.end()) {
                temp.emplace(key, pkgInfo);
                continue;
            }

            auto oldVersion = package::Version::parse(it->second.version);
            if (!oldVersion) {
                LogW("failed to parse old version: {}", oldVersion.error());
                continue;
            }

            auto newVersion = package::Version::parse(pkgInfo.version);
            if (!newVersion) {
                LogW("failed to parse new version: {}", newVersion.error());
                continue;
            }

            if (*oldVersion < *newVersion) {
                it->second = pkgInfo;
            }
        }

        if (temp.empty()) {
            continue;
        }

        std::vector<api::types::v1::PackageInfoV2> filteredPackages;
        filteredPackages.reserve(temp.size());

        for (auto &[_, pkgInfo] : temp) {
            filteredPackages.emplace_back(std::move(pkgInfo));
        }

        auto it = list.find(pkgRepo);
        if (it != list.end()) {
            it->second = std::move(filteredPackages);
        } else {
            list.emplace(pkgRepo, std::move(filteredPackages));
        }
    }
}

utils::error::Result<void> Cli::ensureAuthorized()
{
    LINGLONG_TRACE("ensure authorized");

    auto authReply = this->authorization();
    if (!authReply.isValid()) {
        if (authReply.error().type() == QDBusError::AccessDenied) {
            auto ret = this->runningAsRoot();
            std::string message = "failed to authorize";
            if (!ret) {
                message += ": " + ret.error().message();
            }
            return LINGLONG_ERR(message);
        }

        return LINGLONG_ERR(
          fmt::format("{} {}", authReply.error().message(), authReply.error().name()),
          static_cast<int>(authReply.error().type()));
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::runningAsRoot()
{
    return runningAsRoot(QCoreApplication::instance()->arguments());
}

utils::error::Result<void> Cli::runningAsRoot(const QList<QString> &args)
{
    LINGLONG_TRACE("run with pkexec");

    const char *pkexecBin = "pkexec";
    QStringList argv{ pkexecBin };
    argv.append(args);
    std::vector<char *> targetArgv;
    for (const auto &arg : argv) {
        QByteArray byteArray = arg.toUtf8();
        targetArgv.push_back(strdup(byteArray.constData()));
    }
    LogD("run {}", fmt::join(targetArgv, " "));
    targetArgv.push_back(nullptr);

    auto ret = execvp(pkexecBin, const_cast<char **>(targetArgv.data()));
    // NOTE: if reached here, exevpe is failed.
    for (auto arg : targetArgv) {
        free(arg);
    }
    return LINGLONG_ERR("execve error", ret);
}

QDBusReply<void> Cli::authorization()
{
    // Note: we have marked the method Permissions of PM as rejected.
    // Use this method to determin that this client whether have permission to call PM.
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return QDBusReply<void>{ QDBusError(QDBusError::Failed, pkgMan.error().message().c_str()) };
    }

    return (*pkgMan)->Permissions();
}

utils::error::Result<std::filesystem::path> Cli::ensureCache(runtime::RunContext &context) noexcept
{
    LINGLONG_TRACE("ensure cache via PM");

    const auto &containerID = context.getContainerId();
    auto appLayerItem = context.getCachedAppItem();
    if (!appLayerItem) {
        return LINGLONG_ERR("failed to get cached app item");
    }

    auto appCache = common::dir::getContainerCacheDir(appLayerItem->commit, containerID);
    auto runContextConfigFile = appCache / ".config";
    std::error_code ec;
    if (std::filesystem::exists(runContextConfigFile, ec)) {
        return appCache;
    }
    if (ec) {
        return LINGLONG_ERR(fmt::format("failed to check {}", runContextConfigFile), ec);
    }

    std::optional<QString> pendingJobID;
    bool success = false;
    QEventLoop loop;
    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    if (QObject::connect(*pkgMan,
                         &api::dbus::v1::PackageManager::InitRunContextFinished,
                         &loop,
                         [&success, &loop, &pendingJobID](const QString &taskID, bool taskSuccess) {
                             if (!pendingJobID || taskID != pendingJobID) {
                                 return;
                             }
                             success = taskSuccess;
                             loop.quit();
                         })
        == nullptr) {
        return LINGLONG_ERR("failed to connect InitRunContextFinished signal");
    }

    auto cfgJson = nlohmann::json(context.getConfig()).dump();
    auto reply = (*pkgMan)->InitRunContext(QString::fromStdString(cfgJson),
                                           QString::fromStdString(containerID));
    QDBusPendingReply<QVariantMap> pendingReply = reply;
    auto resultRet = waitDBusReply<api::types::v1::PackageManager1JobInfo>(pendingReply);
    if (!resultRet) {
        return LINGLONG_ERR(resultRet);
    }
    pendingJobID = QString::fromStdString(resultRet->id);

    loop.exec();

    if (!success) {
        return LINGLONG_ERR("InitRunContext failed", utils::error::ErrorCode::Failed);
    }

    return appCache;
}

void Cli::updateAM() noexcept
{
    // NOTE: make sure AM refresh the cache of desktop
    if ((QSysInfo::productType() == "Deepin" || QSysInfo::productType() == "deepin")
        && this->taskState.state == linglong::api::types::v1::State::Succeed) {
        QDBusConnection conn = QDBusConnection::systemBus();
        if (!conn.isConnected()) {
            LogW("Failed to connect to the system bus");
        }

        auto peer = linglong::api::dbus::v1::DBusPeer("org.desktopspec.ApplicationUpdateNotifier1",
                                                      "/org/desktopspec/ApplicationUpdateNotifier1",
                                                      conn);
        auto reply = peer.Ping();
        reply.waitForFinished();
        if (!reply.isValid()) {
            LogW("Failed to ping org.desktopspec.ApplicationUpdateNotifier1: {}",
                 reply.error().message().toStdString());
        }
    }
}

int Cli::inspect(CLI::App *app, const InspectOptions &options)
{
    LINGLONG_TRACE("command inspect");

    auto argsParseFunc = [&app](const std::string &name) -> bool {
        return app->get_subcommand(name)->parsed();
    };

    if (argsParseFunc("dir")) {
        if (options.dirType == "layer") {
            return this->getLayerDir(options);
        } else if (options.dirType == "bundle") {
            return this->getBundleDir(options);
        } else {
            this->printer.printErr(LINGLONG_ERRV(
              fmt::format("Invalid type: {}, type must be layer or bundle", options.dirType)));
            return -1;
        }
    }

    return 0;
}

int Cli::getLayerDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Layer dir");

    auto fuzzyString = options.appid;

    auto fuzzyRef = package::FuzzyReference::parse(fuzzyString);
    if (!fuzzyRef) {
        this->printer.printErr(fuzzyRef.error());
        return -1;
    }

    auto ref = this->repository.clearReference(*fuzzyRef,
                                               { .forceRemote = false, .fallbackToRemote = false });
    if (!ref) {
        LogD("{}", ref.error());
        this->printer.printErr(LINGLONG_ERRV("Can not find such application."));
        return -1;
    }

    std::string module = "binary";
    if (!options.module.empty()) {
        module = options.module;
    }

    auto layerDir = this->repository.getLayerDir(*ref, module);
    if (!layerDir) {
        this->printer.printErr(layerDir.error());
        return -1;
    }

    std::cout << layerDir->path().string() << std::endl;

    return 0;
}

int Cli::getBundleDir(const InspectOptions &options)
{
    LINGLONG_TRACE("Get Bundle dir");

    auto containerIDList = getRunningAppContainers(options.appid);
    if (!containerIDList) {
        this->printer.printErr(containerIDList.error());
        return -1;
    }

    if (containerIDList->empty()) {
        this->printer.printErr(LINGLONG_ERRV("Can not find the running application."));
        return -1;
    }

    auto bundleDir = linglong::common::dir::getBundleDir(containerIDList->front());

    std::cout << bundleDir.string() << std::endl;

    return 0;
}

utils::error::Result<void> Cli::initInteraction()
{
    LINGLONG_TRACE("initInteraction");

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    auto conn = (*pkgMan)->connection();
    auto con = conn.connect((*pkgMan)->service(),
                            (*pkgMan)->path(),
                            (*pkgMan)->interface(),
                            "RequestInteraction",
                            this,
                            SLOT(interaction(QDBusObjectPath, int, QVariantMap)));
    if (!con) {
        return LINGLONG_ERR("Failed to connect signal: RequestInteraction");
    }

    return LINGLONG_OK;
}

utils::error::Result<void> Cli::waitTaskCreated(QDBusPendingReply<QVariantMap> &reply,
                                                TaskType taskType)
{
    LINGLONG_TRACE("waitTaskCreated");

    auto result = waitDBusReply<api::types::v1::PackageManager1PackageTaskResult>(reply);
    if (!result) {
        return LINGLONG_ERR(result.error());
    }

    auto resultCode = static_cast<utils::error::ErrorCode>(result->code);
    if (resultCode != utils::error::ErrorCode::Success) {
        return LINGLONG_ERR(result->message, result->code);
    }

    auto pkgMan = this->getPkgMan();
    if (!pkgMan) {
        return LINGLONG_ERR(pkgMan);
    }

    auto conn = (*pkgMan)->connection();
    this->taskObjectPath = QString::fromStdString(result->taskObjectPath.value());
    this->task = new api::dbus::v1::Task1((*pkgMan)->service(), taskObjectPath, conn);
    this->taskState.state = linglong::api::types::v1::State::Queued;
    this->taskState.taskType = taskType;

    LogD("task object path: {}", this->taskObjectPath.toStdString());

    if (!conn.connect((*pkgMan)->service(),
                      taskObjectPath,
                      "org.freedesktop.DBus.Properties",
                      "PropertiesChanged",
                      this,
                      SLOT(onTaskPropertiesChanged(QString, QVariantMap, QStringList)))) {
        Q_ASSERT(false);
        return LINGLONG_ERR(fmt::format("Failed to connect signal PropertiesChanged: {}",
                                        conn.lastError().message().toStdString()));
    }

    return LINGLONG_OK;
}

void Cli::waitTaskDone()
{
    QEventLoop loop;
    if (QObject::connect(this, &Cli::taskDone, &loop, &QEventLoop::quit) == nullptr) {
        LogE("connect taskDone failed");
        return;
    }
    loop.exec();
}

void Cli::handleInstallError(const utils::error::Error &error,
                             const api::types::v1::PackageManager1InstallParameters &params)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppInstallModuleRequireAppFirst:
        this->printer.printMessage(_("To install the module, one must first install the app."));
        break;
    case utils::error::ErrorCode::AppInstallModuleAlreadyExists:
        this->printer.printMessage(_("Module is already installed."));
        break;
    case utils::error::ErrorCode::AppInstallModuleNotFound:
        this->printer.printMessage(_("The module could not be found remotely."));
        break;
    case utils::error::ErrorCode::AppInstallAlreadyInstalled:
        this->printer.printMessage(
          fmt::format(_("Application already installed, If you want to replace it, try using "
                        "'ll-cli install {} --force'"),
                      params.package.id));
        break;
    case utils::error::ErrorCode::AppInstallNotFoundFromRemote:
        this->printer.printMessage(
          fmt::format(_("Application {} is not found in remote repo."), params.package.id));
        break;
    case utils::error::ErrorCode::AppInstallModuleNoVersion:
        this->printer.printMessage(_("Cannot specify a version when installing a module."));
        break;
    case utils::error::ErrorCode::AppInstallNeedDowngrade:
        this->printer.printMessage(
          fmt::format(_("The latest version has been installed. If you want to "
                        "replace it, try using 'll-cli install {} --force'"),
                      params.package.id));
        break;
    case utils::error::ErrorCode::Unknown:
    case utils::error::ErrorCode::AppInstallFailed:
        this->printer.printMessage(_("Install failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUninstallError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUninstallAppIsRunning: {

        auto ret = this->notifier->notify(api::types::v1::InteractionRequest{
          .appName = "ll-cli",
          .summary = _("The application is currently running and cannot be "
                       "uninstalled. Please turn off the application and try again.") });
        if (!ret) {
            this->printer.printErr(ret.error());
        }
        break;
    }
    case utils::error::ErrorCode::AppUninstallNotFoundFromLocal:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUninstallMultipleVersions:
        this->printer.printMessage(
          fmt::format(_("Multiple versions of the package are installed. Please specify a single "
                        "version to uninstall:\n{}"),
                      error.message()));
        break;
    case utils::error::ErrorCode::AppUninstallBaseOrRuntime:
        this->printer.printMessage(
          _("Base or runtime cannot be uninstalled, please use 'll-cli prune'."));
        break;
    case utils::error::ErrorCode::AppUninstallFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Uninstall failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

void Cli::handleUpgradeError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::AppUpgradeLocalNotFound:
        this->printer.printMessage(_("Application is not installed."));
        break;
    case utils::error::ErrorCode::AppUpgradeFailed:
    case utils::error::ErrorCode::Unknown:
        this->printer.printMessage(_("Upgrade failed"));
        break;
    default:
        if (!handleCommonError(error)) {
            return;
        }
        break;
    }

    if (this->globalOptions.verbose) {
        this->printer.printErr(error);
    }
}

bool Cli::handleCommonError(const utils::error::Error &error)
{
    auto errorCode = static_cast<utils::error::ErrorCode>(error.code());
    switch (errorCode) {
    case utils::error::ErrorCode::NetworkError:
        this->printer.printMessage(_("Network connection failed. Please:"
                                     "\n1. Check your internet connection"
                                     "\n2. Verify network proxy settings if used"));
        break;
    case utils::error::ErrorCode::LayerCompatibilityError:
        this->printer.printMessage(_("Package not found"));
        break;
    case utils::error::ErrorCode::Canceled:
        this->printer.printMessage(_("Operation canceled"));
        break;
    default:
        this->printer.printErr(error);
        return false;
    }

    return true;
}

void Cli::detectDrivers()
{
    QProcess process;
    process.setProgram(QString(LINGLONG_LIBEXEC_DIR "/ll-driver-detect"));
    // 禁用标准输入 (stdin)
    process.setStandardInputFile("/dev/null");
    // 禁用标准输出 (stdout)
    process.setStandardOutputFile("/dev/null");
    // 禁用标准错误输出 (stderr)
    process.setStandardErrorFile("/dev/null");
    process.startDetached();
}

} // namespace linglong::cli
