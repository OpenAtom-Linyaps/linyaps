// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "common/test_process.h"

#include "linglong/utils/finally/finally.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace linglong::test {

TestProcess::TestProcess(std::vector<std::string> arguments)
{
    arguments.insert(arguments.begin(), { "/proc/self/exe", "--test-helper" });
    start(std::move(arguments), {});
}

TestProcess::TestProcess(const std::string &executable,
                         std::vector<std::string> arguments,
                         const std::map<std::string, std::string> &environment)
{
    arguments.insert(arguments.begin(), executable);
    start(std::move(arguments), environment);
}

void TestProcess::start(std::vector<std::string> arguments,
                        const std::map<std::string, std::string> &environment)
{
    std::vector<char *> argv;
    for (auto &argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    std::vector<std::string> environmentValues;
    for (char **current = environ; *current != nullptr; ++current) {
        const std::string_view entry(*current);
        const auto name = entry.substr(0, entry.find('='));
        if (environment.count(std::string(name)) == 0) {
            environmentValues.emplace_back(entry);
        }
    }
    for (const auto &[name, value] : environment) {
        environmentValues.push_back(name + "=" + value);
    }
    std::vector<char *> envp;
    for (auto &entry : environmentValues) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    auto result = ::posix_spawn_file_actions_init(&actions);
    if (result != 0) {
        throw std::runtime_error(std::strerror(result));
    }
    const auto destroyActions = utils::finally::finally([&] {
        ::posix_spawn_file_actions_destroy(&actions);
    });

    const auto output = outputDirectory.path() / "stderr";
    result = ::posix_spawn_file_actions_addopen(&actions,
                                                STDERR_FILENO,
                                                output.c_str(),
                                                O_WRONLY | O_CREAT | O_TRUNC,
                                                0600);
    if (result == 0) {
        const auto stdoutPath = outputDirectory.path() / "stdout";
        result = ::posix_spawn_file_actions_addopen(&actions,
                                                    STDOUT_FILENO,
                                                    stdoutPath.c_str(),
                                                    O_WRONLY | O_CREAT | O_TRUNC,
                                                    0600);
    }
    if (result != 0) {
        throw std::runtime_error(std::strerror(result));
    }

    posix_spawnattr_t attributes;
    result = ::posix_spawnattr_init(&attributes);
    if (result != 0) {
        throw std::runtime_error(std::strerror(result));
    }
    const auto destroyAttributes = utils::finally::finally([&] {
        ::posix_spawnattr_destroy(&attributes);
    });

    result = ::posix_spawnattr_setpgroup(&attributes, 0);
    if (result == 0) {
        result = ::posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    }
    if (result == 0) {
        result = ::posix_spawn(&pid, argv.front(), &actions, &attributes, argv.data(), envp.data());
    }
    if (result != 0) {
        throw std::runtime_error("failed to spawn test process: "
                                 + std::string(std::strerror(result)));
    }
}

TestProcess::~TestProcess()
{
    if (pid > 0) {
        ::kill(-pid, SIGKILL);
        while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) { }
    }
}

int TestProcess::wait(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (pid > 0) {
        int status = 0;
        const auto waited = ::waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            pid = -1;
            exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            return exitCode;
        }
        if (waited < 0 && errno != EINTR) {
            throw std::runtime_error("failed to wait for test process: "
                                     + std::string(std::strerror(errno)));
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return -1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return exitCode;
}

std::string TestProcess::stderrOutput() const
{
    std::ifstream output(outputDirectory.path() / "stderr");
    return { std::istreambuf_iterator<char>(output), std::istreambuf_iterator<char>() };
}

std::string TestProcess::stdoutOutput() const
{
    std::ifstream output(outputDirectory.path() / "stdout");
    return { std::istreambuf_iterator<char>(output), std::istreambuf_iterator<char>() };
}

int runTestHelper(const std::vector<std::string> &arguments)
{
    try {
        if (!arguments.empty()) {
            const std::vector<std::string> parameters(arguments.begin() + 1, arguments.end());
            if (arguments.front() == "filelock") {
                return runFileLockHelper(parameters);
            }
            if (arguments.front() == "builder") {
                return runBuilderHelper(parameters);
            }
            if (arguments.front() == "source-fetch") {
                return runSourceFetchHelper(parameters);
            }
        }

        std::cerr << "unknown or missing test helper\n";
    } catch (const std::exception &error) {
        std::cerr << "test helper failed: " << error.what() << '\n';
    }

    return 2;
}

} // namespace linglong::test
