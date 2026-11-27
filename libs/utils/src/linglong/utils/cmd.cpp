/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cmd.h"

#include "configure.h"
#include "linglong/common/error.h"
#include "linglong/common/strings.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/io/event_loop.h"
#include "linglong/utils/io/pipe.h"
#include "linglong/utils/log/log.h"

#include <fmt/ranges.h>
#include <sys/epoll.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <optional>
#include <string_view>
#include <thread>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {
namespace {

using Deadline = std::optional<std::chrono::steady_clock::time_point>;

int remainingMilliseconds(const Deadline &deadline) noexcept
{
    if (!deadline) {
        return -1;
    }
    const auto remaining =
      std::chrono::ceil<std::chrono::milliseconds>(*deadline - std::chrono::steady_clock::now())
        .count();
    return static_cast<int>(
      std::clamp<std::chrono::milliseconds::rep>(remaining, 0, std::numeric_limits<int>::max()));
}

class ChildProcess
{
public:
    ChildProcess(pid_t pid, bool processGroup) noexcept
        : pid_(pid)
        , processGroup_(processGroup)
    {
    }

    ChildProcess(const ChildProcess &) = delete;
    ChildProcess &operator=(const ChildProcess &) = delete;

    ~ChildProcess()
    {
        if (pid_ <= 0) {
            return;
        }
        if (processGroup_) {
            ::kill(-pid_, SIGKILL);
        }
        // The child may not have created its process group yet.
        ::kill(pid_, SIGKILL);
        while (::waitpid(pid_, nullptr, 0) == -1 && errno == EINTR) { }
    }

    utils::error::Result<int> wait(const Deadline &deadline) noexcept
    {
        LINGLONG_TRACE("wait for command exit");
        int status = 0;
        for (;;) {
            const auto waited = ::waitpid(pid_, &status, deadline ? WNOHANG : 0);
            if (waited == pid_) {
                pid_ = -1;
                return status;
            }
            if (waited == -1) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == ECHILD) {
                    pid_ = -1;
                }
                return LINGLONG_ERR(
                  fmt::format("waitpid error: {}", common::error::errorString(errno)));
            }
            const auto remaining = remainingMilliseconds(deadline);
            if (remaining == 0) {
                return LINGLONG_ERR("waiting for command exit timed out", ETIMEDOUT);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min(remaining, 10)));
        }
    }

private:
    pid_t pid_;
    bool processGroup_;
};

std::vector<std::string> commandEnvironment(const std::map<std::string, std::string> &overrides)
{
    std::vector<std::string> environment;
    for (char *const *env = environ; *env != nullptr; ++env) {
        environment.emplace_back(*env);
    }
    for (const auto &[name, value] : overrides) {
        const auto prefix = name + '=';
        const auto matches = [&prefix](const auto &entry) {
            return entry.compare(0, prefix.size(), prefix) == 0;
        };
        if (value.empty()) {
            environment.erase(std::remove_if(environment.begin(), environment.end(), matches),
                              environment.end());
        } else {
            const auto found = std::find_if(environment.begin(), environment.end(), matches);
            if (found == environment.end()) {
                environment.push_back(prefix + value);
            } else {
                *found = prefix + value;
            }
        }
    }
    return environment;
}

std::vector<char *> stringPointers(std::vector<std::string> &strings)
{
    std::vector<char *> pointers;
    pointers.reserve(strings.size() + 1);
    for (auto &entry : strings) {
        pointers.push_back(entry.data());
    }
    pointers.push_back(nullptr);
    return pointers;
}

utils::error::Result<pid_t> startCommand(const std::filesystem::path &path,
                                         char *const *argv,
                                         char *const *envp,
                                         io::PipeSet &pipes,
                                         bool processGroup) noexcept
{
    LINGLONG_TRACE("start command");
    const auto *command = path.c_str();
    const auto pid = ::fork();
    if (pid == -1) {
        return LINGLONG_ERR(fmt::format("fork error: {}", common::error::errorString(errno)));
    }
    if (pid == 0) {
        if (processGroup && ::setpgid(0, 0) == -1) {
            ::_exit(EXIT_FAILURE);
        }
        if (!pipes.setupChildStdio()) {
            ::_exit(EXIT_FAILURE);
        }
        ::execve(command, argv, envp);
        constexpr char message[] = "execve failed\n";
        ::write(STDERR_FILENO, message, sizeof(message) - 1);
        ::_exit(EXIT_FAILURE);
    }
    return pid;
}

utils::error::Result<void> communicate(io::Pipe &input,
                                       io::Pipe &output,
                                       const std::string &content,
                                       const Deadline &deadline,
                                       std::string &result) noexcept
{
    LINGLONG_TRACE("exchange command input and output");
    for (const auto fd : { input.writeEnd(), output.readEnd() }) {
        const auto flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == -1 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            return LINGLONG_ERR(
              fmt::format("set non block error: {}", common::error::errorString(errno)));
        }
    }
    auto loop = io::EventLoop::create(2);
    if (!loop) {
        return LINGLONG_ERR(loop);
    }
    auto added = loop->add(output.readEnd(), EPOLLIN);
    if (!added) {
        return LINGLONG_ERR(added);
    }
    if (!*added) {
        return LINGLONG_ERR("stdout pipe cannot be monitored");
    }
    if (content.empty()) {
        input.closeWriteEnd();
    } else {
        added = loop->add(input.writeEnd(), EPOLLOUT);
        if (!added) {
            return LINGLONG_ERR(added);
        }
        if (!*added) {
            return LINGLONG_ERR("stdin pipe cannot be monitored");
        }
    }

    std::size_t written = 0;
    std::array<char, 8192> buffer{};
    while (input || output) {
        const auto remaining = remainingMilliseconds(deadline);
        if (remaining == 0) {
            return LINGLONG_ERR("command input/output timed out", ETIMEDOUT);
        }
        auto ready = loop->wait(remaining);
        if (!ready) {
            return LINGLONG_ERR(ready);
        }
        for (int i = 0; i < ready->count; ++i) {
            const auto fd = ready->events[i].data.fd;
            if (fd == output.readEnd()) {
                const auto size = ::read(fd, buffer.data(), buffer.size());
                if (size > 0) {
                    result.append(buffer.data(), static_cast<std::size_t>(size));
                } else if (size == 0 || (errno != EINTR && errno != EAGAIN)) {
                    loop->remove(fd);
                    output.closeReadEnd();
                }
            } else if (fd == input.writeEnd()) {
                const auto size = ::write(fd, content.data() + written, content.size() - written);
                if (size < 0) {
                    const auto error = errno;
                    if (error == EINTR || error == EAGAIN) {
                        continue;
                    }
                    if (error != EPIPE) {
                        return LINGLONG_ERR("failed to write command input", error);
                    }
                } else if (size > 0) {
                    written += static_cast<std::size_t>(size);
                }
                if (size <= 0 || written == content.size()) {
                    loop->remove(fd);
                    input.closeWriteEnd();
                }
            }
        }
    }
    return LINGLONG_OK;
}

std::string timeoutMessage(std::chrono::milliseconds timeout,
                           const std::filesystem::path &path,
                           const std::string &output)
{
    auto message = fmt::format("command timed out after {} ms: {}", timeout.count(), path);
    if (!output.empty()) {
        constexpr std::size_t limit = 4096;
        message += fmt::format("\npartial stdout: {}{}",
                               std::string_view(output).substr(0, limit),
                               output.size() > limit ? " [truncated]" : "");
    }
    return message;
}

utils::error::Result<std::string> commandResult(int status, std::string output)
{
    LINGLONG_TRACE("check command result");
    if (WIFEXITED(status)) {
        const auto exitCode = WEXITSTATUS(status);
        if (exitCode == 0) {
            return output;
        }
        return LINGLONG_ERR(
          fmt::format("command execute failed with exit code {}: {}", exitCode, output));
    }
    if (WIFSIGNALED(status)) {
        return LINGLONG_ERR(fmt::format("command killed by signal: {}", WTERMSIG(status)));
    }
    return LINGLONG_ERR("command exited abnormally");
}

} // namespace

Cmd::Cmd(std::string command) noexcept
    : m_command(std::move(command))
{
}

Cmd::~Cmd() = default;

bool Cmd::exists() noexcept
{
    return !getCommandPath().empty();
}

std::filesystem::path Cmd::getCommandPath()
{
    // Check if command is an absolute path
    std::error_code ec;
    std::filesystem::path path{ m_command };
    if (path.is_absolute() && std::filesystem::exists(path, ec)) {
        return path;
    }

    // Search in PATH environment variable
    std::vector<std::string> pathDirs = { BINDIR };
    const char *pathEnv = std::getenv("PATH");
    if (pathEnv && pathEnv[0] != '\0') {
        auto dirs = common::strings::split(pathEnv, ':', common::strings::splitOption::SkipEmpty);
        pathDirs.insert(pathDirs.end(), dirs.begin(), dirs.end());
    } else {
        pathDirs.insert(pathDirs.end(), { "/usr/local/bin", "/usr/bin", "/bin" });
    }

    for (const auto &pathDir : pathDirs) {
        std::filesystem::path fullPath = std::filesystem::path{ pathDir } / m_command;
        if (std::filesystem::exists(fullPath, ec)
            && std::filesystem::is_regular_file(fullPath, ec)) {
            return fullPath;
        }
    }

    return {};
}

utils::error::Result<std::string> Cmd::exec(const std::vector<std::string> &args) noexcept
{
    LINGLONG_TRACE(fmt::format("exec cmd: {} args: {}", m_command, fmt::join(args, ", ")));

    auto commandPath = getCommandPath();
    if (commandPath.empty()) {
        return LINGLONG_ERR(fmt::format("command not found: {}", m_command));
    }

    auto pipes = io::PipeSet::create(O_CLOEXEC, io::PipeSet::Stdin | io::PipeSet::Stdout);
    if (!pipes) {
        return LINGLONG_ERR(fmt::format("pipe error: {}", common::error::errorString(errno)));
    }

    auto environment = commandEnvironment(m_envs);
    auto envp = stringPointers(environment);

    std::vector<std::string> arguments{ commandPath.filename().string() };
    arguments.insert(arguments.end(), args.begin(), args.end());
    auto argv = stringPointers(arguments);

    LogD("execute {} with args [{}]", commandPath, fmt::join(args, ", "));

    const bool createProcessGroup = m_timeout > std::chrono::milliseconds::zero();
    Deadline deadline;
    if (m_timeout > std::chrono::milliseconds::zero()) {
        const auto now = std::chrono::steady_clock::now();
        const auto maximum = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::time_point::max() - now);
        deadline = now + std::min(m_timeout, maximum);
    }
    auto started = startCommand(commandPath, argv.data(), envp.data(), *pipes, createProcessGroup);
    if (!started) {
        return LINGLONG_ERR(started);
    }
    ChildProcess child(*started, createProcessGroup);
    pipes->closeChildEnds();

    // Block only in the parent so the executed command keeps the caller's signal mask.
    sigset_t blocked;
    ::sigemptyset(&blocked);
    ::sigaddset(&blocked, SIGPIPE);
    sigset_t original;
    const auto maskError = ::pthread_sigmask(SIG_BLOCK, &blocked, &original);
    if (maskError != 0) {
        return LINGLONG_ERR("failed to block SIGPIPE", maskError);
    }
    sigset_t pending;
    if (::sigpending(&pending) == -1) {
        const auto error = errno;
        ::pthread_sigmask(SIG_SETMASK, &original, nullptr);
        return LINGLONG_ERR("failed to inspect pending signals", error);
    }
    const bool alreadyPending = ::sigismember(&pending, SIGPIPE) == 1;
    const auto restoreSigpipe = finally::finally([&] {
        if (!alreadyPending) {
            // Drain a newly pending SIGPIPE before restoring the caller's signal mask.
            const timespec timeout{};
            while (::sigtimedwait(&blocked, nullptr, &timeout) == -1 && errno == EINTR) { }
        }
        ::pthread_sigmask(SIG_SETMASK, &original, nullptr);
    });

    std::string output;
    auto result =
      communicate(*pipes->stdinPipe, *pipes->stdoutPipe, m_stdinContent, deadline, output);
    if (!result) {
        if (result.error().code() == ETIMEDOUT) {
            return LINGLONG_ERR(timeoutMessage(m_timeout, commandPath, output), ETIMEDOUT);
        }
        return LINGLONG_ERR(result);
    }
    auto status = child.wait(deadline);
    if (!status) {
        if (status.error().code() == ETIMEDOUT) {
            return LINGLONG_ERR(timeoutMessage(m_timeout, commandPath, output), ETIMEDOUT);
        }
        return LINGLONG_ERR(status);
    }
    return commandResult(*status, std::move(output));
}

Cmd &Cmd::setEnv(const std::string &name, const std::string &value) noexcept
{
    // Store the environment variable (empty value means unset)
    m_envs[name] = value;
    return *this;
}

Cmd &Cmd::toStdin(std::string content) noexcept
{
    m_stdinContent = std::move(content);
    return *this;
}

Cmd &Cmd::setTimeout(std::chrono::milliseconds timeout) noexcept
{
    m_timeout = timeout;
    return *this;
}

} // namespace linglong::utils
