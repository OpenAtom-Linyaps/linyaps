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
#include "linglong/utils/log/log.h"

#include <fmt/ranges.h>
#include <sys/epoll.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {

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
    LINGLONG_TRACE(fmt::format("exec cmd: {} args: {}", m_command, fmt::join(args, " ")));

    auto commandPath = getCommandPath();
    if (commandPath.empty()) {
        return LINGLONG_ERR(fmt::format("command not found: {}", m_command));
    }

    int stdoutPipe[2];
    if (pipe(stdoutPipe) == -1) {
        return LINGLONG_ERR(fmt::format("pipe error: {}", common::error::errorString(errno)));
    }
    auto stdoutPipeCloser = utils::finally::finally([stdoutPipe]() {
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
    });

    int stdinPipe[2];
    if (pipe(stdinPipe) == -1) {
        return LINGLONG_ERR(fmt::format("pipe error: {}", common::error::errorString(errno)));
    }
    auto stdinPipeCloser = utils::finally::finally([stdinPipe]() {
        close(stdinPipe[0]);
        close(stdinPipe[1]);
    });

    // Pre-allocate environment variables before fork to avoid memory allocation issues
    std::vector<std::string> envStrings;
    std::vector<char *> envp;

    // Copy current environment
    for (char **env = environ; *env != nullptr; ++env) {
        envStrings.emplace_back(*env);
    }

    // Add or override environment variables
    for (const auto &[name, value] : m_envs) {
        if (value.empty()) {
            // Remove the environment variable if value is empty
            envStrings.erase(std::remove_if(envStrings.begin(),
                                            envStrings.end(),
                                            [&name](const std::string &env) {
                                                return env.size() > name.size()
                                                  && env.substr(0, name.size()) == name
                                                  && env[name.size()] == '=';
                                            }),
                             envStrings.end());
        } else {
            // Add or update environment variable
            std::string envVar = name + "=" + value;
            bool found = false;
            for (auto &env : envStrings) {
                if (env.size() > name.size() && env.substr(0, name.size()) == name
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

    pid_t pid = fork();
    if (pid == -1) {
        return LINGLONG_ERR(fmt::format("fork error: {}", common::error::errorString(errno)));
    }

    // child process
    if (pid == 0) {
        // Redirect stdout and stdin
        close(stdoutPipe[0]);
        close(stdinPipe[1]);
        if (dup2(stdoutPipe[1], STDOUT_FILENO) == -1 || dup2(stdinPipe[0], STDIN_FILENO) == -1) {
            exit(1);
        }
        close(stdoutPipe[1]);
        close(stdinPipe[0]);

        std::vector<char *> argv;
        auto filename = commandPath.filename();
        argv.push_back(const_cast<char *>(filename.c_str()));
        for (const auto &arg : args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvpe(commandPath.c_str(), argv.data(), envp.data());

        std::cout << "execvpe failed: " << common::error::errorString(errno) << std::endl;

        _exit(1);
    }

    // parent process
    close(stdoutPipe[1]);
    close(stdinPipe[0]);

    auto setNonBlock = [](int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) {
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
        }
        return false;
    };
    if (!setNonBlock(stdoutPipe[0]) || !setNonBlock(stdinPipe[1])) {
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

    ev.events = EPOLLIN;
    ev.data.fd = stdoutPipe[0];
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, stdoutPipe[0], &ev) == -1) {
        return LINGLONG_ERR(
          fmt::format("epoll_ctl stdout error: {}", common::error::errorString(errno)));
    }
    activeFds++;

    size_t writtenBytes = 0;
    if (!m_stdinContent.empty()) {
        ev.events = EPOLLOUT;
        ev.data.fd = stdinPipe[1];
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, stdinPipe[1], &ev) == -1) {
            return LINGLONG_ERR(
              fmt::format("epoll_ctl stdin error: {}", common::error::errorString(errno)));
        }
        activeFds++;
    } else {
        // If no input, close write end immediately to send EOF to child
        close(stdinPipe[1]);
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
            if (fd == stdoutPipe[0]) {
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
            } else if (fd == stdinPipe[1]) {
                if (writtenBytes < m_stdinContent.size()) {
                    ssize_t n = write(fd,
                                      m_stdinContent.data() + writtenBytes,
                                      m_stdinContent.size() - writtenBytes);
                    if (n > 0) {
                        writtenBytes += n;
                    } else if (n == -1) {
                        if (errno == EINTR) {
                            continue;
                        }

                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                            activeFds--;
                        }
                    }
                }

                if (writtenBytes >= m_stdinContent.size()) {
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
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

    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 0) {
            return output;
        } else {
            return LINGLONG_ERR(
              fmt::format("command execute failed with exit code {}: {}", exitCode, output));
        }
    } else if (WIFSIGNALED(status)) {
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
