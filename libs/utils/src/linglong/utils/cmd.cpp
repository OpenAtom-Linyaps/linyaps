/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "cmd.h"

#include "configure.h"
#include "linglong/common/error.h"
#include "linglong/common/strings.h"
#include "linglong/utils/finally/finally.h"
#include "linglong/utils/io/pipe.h"
#include "linglong/utils/log/log.h"

#include <fmt/ranges.h>
#include <sys/epoll.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {

namespace {

// Writing to a pipe (or socket) whose reader has already gone away normally
// raises SIGPIPE, and its default disposition terminates the process from
// inside write(2), before the failing call has a chance to return EPIPE.
// Cmd::exec feeds a child that is free to exit without draining stdin - an
// executable OCI config patch that skips its input, for instance - so without
// this guard an ordinary child could kill its caller instead of producing a
// failed Result.
//
// The previous disposition is saved and restored around the call so that
// running a command does not permanently change the signal setup of the
// process; when SIGPIPE is already ignored (applicationInitialize() does that)
// the guard is effectively a no-op.
class ScopedIgnoreSIGPIPE
{
public:
    ScopedIgnoreSIGPIPE() noexcept
    {
        struct sigaction action{};
        action.sa_handler = SIG_IGN;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        installed_ = (sigaction(SIGPIPE, &action, &previous_) == 0);
    }

    ScopedIgnoreSIGPIPE(const ScopedIgnoreSIGPIPE &) = delete;
    ScopedIgnoreSIGPIPE &operator=(const ScopedIgnoreSIGPIPE &) = delete;

    ~ScopedIgnoreSIGPIPE() noexcept
    {
        if (installed_) {
            sigaction(SIGPIPE, &previous_, nullptr);
        }
    }

private:
    bool installed_{ false };
    struct sigaction previous_{};
};

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

// Run the command and capture its stdout.
//
// stdin and stdout are connected through pipes whose ownership is split across
// fork(): the parent keeps the read end of stdout and the write end of stdin,
// while the child gets the write end of stdout and the read end of stdin. Each
// side closes the end it does not use, so the child observes EOF on stdin once
// m_stdinContent has been written, and the parent observes EOF on stdout once
// the child is gone.
utils::error::Result<std::string> Cmd::exec(const std::vector<std::string> &args) noexcept
{
    LINGLONG_TRACE(fmt::format("exec cmd: {} args: {}", m_command, fmt::join(args, " ")));

    // The child is free to stop reading (or close) its stdin before we finish
    // writing m_stdinContent into it: an executable OCI config patch, for
    // example, may exit right away or bail out on malformed input long before
    // an input that is larger than the pipe buffer has been drained. The
    // resulting write(2) to the orphaned pipe raises SIGPIPE, whose default
    // disposition kills the caller from inside write(2) before EPIPE can be
    // observed. Ignore it for the duration of the call and restore the previous
    // disposition on return, so exec() always reports a failed Result instead of
    // terminating a caller that treats a failing command as a skippable item.
    ScopedIgnoreSIGPIPE sigpipeGuard;

    auto commandPath = getCommandPath();
    if (commandPath.empty()) {
        return LINGLONG_ERR(fmt::format("command not found: {}", m_command));
    }

    // Pipe ownership is split across fork():
    //   * stdoutPipe: the parent owns the read end and collects the child
    //     output, the child owns the write end which becomes its STDOUT_FILENO.
    //     The parent closes the write end, otherwise read() on the parent side
    //     never sees EOF because the parent itself keeps a writer open.
    //   * stdinPipe: the parent owns the write end and feeds m_stdinContent,
    //     the child owns the read end which becomes its STDIN_FILENO. The parent
    //     closes the read end so the child stays the only reader: as soon as the
    //     child stops reading, write() fails with EPIPE instead of silently
    //     filling the pipe buffer forever.
    // io::Pipe is backed by UniqueFd, so every descriptor is closed exactly once
    // and a descriptor number released earlier cannot be closed again after
    // another thread recycled it.
    auto stdoutPipe = utils::io::Pipe::create(O_CLOEXEC);
    if (!stdoutPipe) {
        return LINGLONG_ERR(fmt::format("pipe error: {}", common::error::errorString(errno)));
    }

    auto stdinPipe = utils::io::Pipe::create(O_CLOEXEC);
    if (!stdinPipe) {
        return LINGLONG_ERR(fmt::format("pipe error: {}", common::error::errorString(errno)));
    }

    // Pre-allocate environment variables before fork to avoid memory allocation issues
    std::vector<std::string> envStrings;
    std::vector<char *> envp;

    // Copy current environment
    for (char *const *env = environ; *env != nullptr; ++env) {
        envStrings.emplace_back(*env);
    }

    // Add or override environment variables
    for (auto it = m_envs.cbegin(); it != m_envs.cend(); ++it) {
        const auto &name = it->first;
        const auto &value = it->second;
        if (value.empty()) {
            // Remove the environment variable if value is empty
            envStrings.erase(std::remove_if(envStrings.begin(),
                                            envStrings.end(),
                                            [&name](const std::string &env) {
                                                return env.size() > name.size()
                                                  && env.compare(0, name.size(), name) == 0
                                                  && env[name.size()] == '=';
                                            }),
                             envStrings.end());
        } else {
            // Add or update environment variable
            const auto envVar = (name + "=").append(value);
            bool found = false;
            for (auto &env : envStrings) {
                if (env.size() > name.size() && env.compare(0, name.size(), name) == 0
                    && env[name.size()] == '=') {
                    env = envVar;
                    found = true;
                    break;
                }
            }
            if (!found) {
                envStrings.push_back(envVar);
            }
        }
    }

    // Build envp array for execvpe
    envp.reserve(envStrings.size() + 1);
    for (auto &env : envStrings) {
        envp.push_back(const_cast<char *>(env.c_str()));
    }
    envp.push_back(nullptr);

    LogD("execute {} with args [{}]", commandPath, fmt::join(args, ", "));

    const pid_t pid = fork();
    if (pid == -1) {
        return LINGLONG_ERR(fmt::format("fork error: {}", common::error::errorString(errno)));
    }

    // child process
    if (pid == 0) {
        // Redirect stdout and stdin, then drop the ends that belong to the
        // parent so that EOF reaches the peer exactly once.
        stdoutPipe->closeReadEnd();
        stdinPipe->closeWriteEnd();
        if (dup2(stdoutPipe->writeEnd(), STDOUT_FILENO) == -1
            || dup2(stdinPipe->readEnd(), STDIN_FILENO) == -1) {
            _exit(EXIT_FAILURE);
        }
        stdoutPipe->closeWriteEnd();
        stdinPipe->closeReadEnd();

        std::vector<char *> argv;
        auto filename = commandPath.filename();
        argv.push_back(const_cast<char *>(filename.c_str()));
        for (const auto &arg : args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvpe(commandPath.c_str(), argv.data(), envp.data());

        std::cout << "execvpe failed: " << common::error::errorString(errno) << std::endl;

        _exit(EXIT_FAILURE);
    }

    // parent process: keep only the ends this process reads from and writes to
    stdoutPipe->closeWriteEnd();
    stdinPipe->closeReadEnd();

    auto setNonBlock = [](int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) {
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
        }
        return false;
    };
    if (!setNonBlock(stdoutPipe->readEnd()) || !setNonBlock(stdinPipe->writeEnd())) {
        return LINGLONG_ERR(
          fmt::format("set non block error: {}", common::error::errorString(errno)));
    }

    int epfd = epoll_create1(O_CLOEXEC);
    if (epfd == -1) {
        return LINGLONG_ERR(
          fmt::format("epoll_create error: {}", common::error::errorString(errno)));
    }
    auto epfdCloser = utils::finally::finally([epfd]() {
        close(epfd);
    });

    struct epoll_event ev;
    int activeFds = 0;

    const int stdoutFd = stdoutPipe->readEnd();
    const int stdinFd = stdinPipe->writeEnd();

    ev.events = EPOLLIN;
    ev.data.fd = stdoutFd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, stdoutFd, &ev) == -1) {
        return LINGLONG_ERR(
          fmt::format("epoll_ctl stdout error: {}", common::error::errorString(errno)));
    }
    activeFds++;

    size_t writtenBytes = 0;
    // Set when the child stops reading (or closes) its stdin before all of
    // m_stdinContent could be handed over; reported once the child is reaped.
    bool brokenPipe = false;
    // errno of the failing write, so the error below can tell a reader that
    // vanished (EPIPE) apart from any other write error.
    int stdinWriteError = 0;

    if (!m_stdinContent.empty()) {
        ev.events = EPOLLOUT;
        ev.data.fd = stdinFd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, stdinFd, &ev) == -1) {
            return LINGLONG_ERR(
              fmt::format("epoll_ctl stdin error: {}", common::error::errorString(errno)));
        }
        activeFds++;
    } else {
        // If no input, close write end immediately to send EOF to child
        stdinPipe->closeWriteEnd();
    }

    std::string output;
    std::vector<char> buffer(4096);
    const int MAX_EVENTS = 2;
    struct epoll_event events[MAX_EVENTS];

    while (activeFds > 0) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno == EINTR)
                continue;
            return LINGLONG_ERR(
              fmt::format("epoll_wait error: {}", common::error::errorString(errno)));
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            if (fd == stdoutFd) {
                while (true) {
                    ssize_t n = read(fd, buffer.data(), buffer.size());
                    if (n > 0) {
                        output.append(buffer.data(), static_cast<size_t>(n));
                        continue;
                    }

                    if (n == -1 && errno == EINTR) {
                        continue;
                    }

                    if ((n == -1 && errno != EAGAIN && errno != EWOULDBLOCK) || n == 0) {
                        // error or EOF, stop monitoring this FD
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        activeFds--;
                    }
                    break;
                }
            } else if (fd == stdinFd) {
                // The child can close its stdin, or simply stop reading it, at
                // any moment. A pipe without a reader reports EPOLLERR/EPOLLHUP
                // in addition to EPOLLOUT, so look at the event mask first and
                // never write into a pipe that has no reader left.
                if ((events[i].events & (EPOLLERR | EPOLLHUP)) != 0) {
                    stdinWriteError = EPIPE;
                    brokenPipe = true;
                } else {
                    while (writtenBytes < m_stdinContent.size()) {
                        ssize_t n = write(fd,
                                          m_stdinContent.data() + writtenBytes,
                                          m_stdinContent.size() - writtenBytes);
                        if (n > 0) {
                            writtenBytes += static_cast<size_t>(n);
                            continue;
                        }

                        if (n == -1 && errno == EINTR) {
                            continue;
                        }

                        if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // The pipe buffer is full; come back once the child
                            // has read enough for another write to progress.
                            break;
                        }

                        if (n == 0) {
                            // A zero length write on a pipe cannot make any
                            // progress, treat it like a vanished reader instead
                            // of spinning forever.
                            stdinWriteError = EPIPE;
                        } else {
                            stdinWriteError = errno;
                        }

                        brokenPipe = true;
                        break;
                    }
                }

                if (brokenPipe || writtenBytes >= m_stdinContent.size()) {
                    // Stop monitoring the write end and close it. io::Pipe owns
                    // the descriptor, so it is closed exactly once; closing it
                    // is also what sends EOF to a child that read everything.
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    stdinPipe->closeWriteEnd();
                    activeFds--;
                }
            }
        }
    }

    int status;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno == EINTR)
            continue;
        return LINGLONG_ERR(fmt::format("waitpid error: {}", common::error::errorString(errno)));
    }

    if (brokenPipe) {
        // The child stopped reading stdin before we could hand over all of
        // m_stdinContent. Report it as a failed Result together with the child
        // state and the output it did produce: callers like the executable OCI
        // config patches treat a failing command as a skippable item, and that
        // must remain a normal failure instead of a crash or a silent success.
        std::string childState = "abnormal exit";
        if (WIFEXITED(status)) {
            childState = fmt::format("exit code {}", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            childState = fmt::format("killed by signal {}", WTERMSIG(status));
        }

        const std::string reason = stdinWriteError == EPIPE
          ? std::string{ "child closed stdin (broken pipe)" }
          : fmt::format("failed to write to child stdin: {}",
                        common::error::errorString(stdinWriteError));

        return LINGLONG_ERR(fmt::format("{} while writing {} of {} bytes, {}: {}",
                                        reason,
                                        writtenBytes,
                                        m_stdinContent.size(),
                                        childState,
                                        output));
    }

    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
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

} // namespace linglong::utils
