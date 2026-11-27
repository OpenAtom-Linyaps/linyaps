// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/finally/finally.h"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

TEST(command, Exec)
{
    auto ret = linglong::utils::Cmd("echo").exec({ "-n", "hello" });
    EXPECT_TRUE(ret);
    EXPECT_EQ(ret->size(), 5);
    EXPECT_EQ(*ret, "hello");
    auto userId = linglong::utils::Cmd("id").exec({ "-u" });
    EXPECT_TRUE(userId.has_value());

    userId = userId->substr(0, userId->find('\n'));
    EXPECT_EQ(*userId, std::to_string(getuid()));

    // 测试command不存在时
    auto ret3 = linglong::utils::Cmd("nonexistent").exec();
    EXPECT_FALSE(ret3.has_value());

    // 测试exec出错时
    auto ret4 = linglong::utils::Cmd("ls").exec({ "/linglong/nonexistent" });
    EXPECT_FALSE(ret4.has_value());
}

TEST(command, setEnv)
{
    linglong::utils::Cmd cmd("bash");
    // test set
    cmd.setEnv("LINGLONG_TEST_SETENV", "OK");
    auto existsRef = cmd.exists();
    EXPECT_TRUE(existsRef);
    // test unset
    cmd.setEnv("PATH", "");
    auto ret = cmd.exec({ "-c", "export" });
    EXPECT_TRUE(ret.has_value()) << "failed to execute export command: " << ret.error().message();
    EXPECT_TRUE(ret->find("declare -x LINGLONG_TEST_SETENV=") != std::string::npos) << *ret;
    EXPECT_FALSE(ret->find("declare -x PATH=\"") != std::string::npos) << *ret;
}

TEST(command, toStdin)
{
    // Test writing to stdin using cat command
    auto ret = linglong::utils::Cmd("cat").toStdin("Hello, World!").exec();

    EXPECT_TRUE(ret.has_value()) << "failed to execute cat command: " << ret.error().message();
    EXPECT_EQ(*ret, "Hello, World!");

    // Test with multiline input
    auto ret2 = linglong::utils::Cmd("wc").toStdin("line1\nline2\nline3\n").exec({ "-l" });

    EXPECT_TRUE(ret2.has_value()) << "failed to execute wc command: " << ret2.error().message();
    // wc -l should output " 3" for 3 lines
    EXPECT_TRUE(ret2->find("3") != std::string::npos) << *ret2;

    // Test empty stdin
    auto ret3 = linglong::utils::Cmd("cat").toStdin("").exec();

    EXPECT_TRUE(ret3.has_value()) << "failed to execute cat with empty stdin: "
                                  << ret3.error().message();
    EXPECT_TRUE(ret3->empty()) << "cat with empty stdin should return empty output";

    // Test with data exceeding 1M
    std::string large_data(1048576 + 1024, 'A');
    auto ret4 = linglong::utils::Cmd("wc").toStdin(large_data).exec({ "-c" });

    EXPECT_TRUE(ret4.has_value()) << "failed to execute wc with large stdin: "
                                  << ret4.error().message();
    EXPECT_TRUE(ret4->find("1049600") != std::string::npos)
      << "wc -c should report 1049600 bytes, got: " << *ret4;
}

TEST(command, EarlyStdinClosurePreservesSigpipeState)
{
    for (const auto timeout : { 0, 2000, 100 }) {
        for (int signalState = 0; signalState < 3; ++signalState) {
            const bool blocked = signalState != 0;
            const bool pending = signalState == 2;
            SCOPED_TRACE(timeout);
            SCOPED_TRACE(signalState);
            ASSERT_EXIT(
              {
                  struct sigaction action{};
                  action.sa_handler = SIG_DFL;
                  ::sigemptyset(&action.sa_mask);
                  if (::sigaction(SIGPIPE, &action, nullptr) == -1) {
                      ::_exit(EXIT_FAILURE);
                  }
                  sigset_t mask;
                  ::sigemptyset(&mask);
                  if (blocked) {
                      ::sigaddset(&mask, SIGPIPE);
                  }
                  if (::pthread_sigmask(SIG_SETMASK, &mask, nullptr) != 0
                      || (pending && ::raise(SIGPIPE) != 0)) {
                      ::_exit(EXIT_FAILURE);
                  }
                  const auto result =
                    linglong::utils::Cmd("sh")
                      .setTimeout(std::chrono::milliseconds(timeout))
                      .toStdin(std::string(1024 * 1024, 'x'))
                      .exec({ "-c",
                              timeout == 100 ? "exec 0<&-; printf success; exec 1>&-; exec sleep 10"
                                             : "exec 0<&-; printf success" });
                  sigset_t after;
                  if (::pthread_sigmask(SIG_SETMASK, nullptr, &after) != 0
                      || (::sigismember(&after, SIGPIPE) == 1) != blocked
                      || ::sigpending(&after) == -1
                      || (::sigismember(&after, SIGPIPE) == 1) != pending
                      || ::sigaction(SIGPIPE, nullptr, &action) == -1
                      || action.sa_handler != SIG_DFL) {
                      ::_exit(EXIT_FAILURE);
                  }
                  const bool expected = timeout == 100
                    ? !result && result.error().code() == ETIMEDOUT
                    : result && *result == "success";
                  ::_exit(expected ? EXIT_SUCCESS : EXIT_FAILURE);
              },
              ::testing::ExitedWithCode(EXIT_SUCCESS),
              "");
        }
    }
}

TEST(command, NonzeroExitIncludesOutput)
{
    const auto result = linglong::utils::Cmd("sh").exec({ "-c", "printf diagnostic; exit 7" });
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("exit code 7: diagnostic"), std::string::npos);
}

TEST(command, ReportsSignalTermination)
{
    const auto result = linglong::utils::Cmd("sh").exec({ "-c", "kill -TERM $$" });
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("command killed by signal: " + std::to_string(SIGTERM)),
              std::string::npos);
}

TEST(command, ExecFailureReportsToStderr)
{
    TempDir directory;
    const auto script = directory.path() / "script";
    std::ofstream(script) << "#!/bin/sh\nprintf unused\n";
    ASSERT_EQ(::chmod(script.c_str(), 0600), 0);
    ASSERT_EXIT(
      {
          const auto result = linglong::utils::Cmd(script.string()).exec();
          ::_exit(!result && result.error().message().find("exit code 1:") != std::string::npos
                    ? EXIT_SUCCESS
                    : EXIT_FAILURE);
      },
      ::testing::ExitedWithCode(EXIT_SUCCESS),
      "execve failed");
}

TEST(command, PreservesInputAndOutputWithClosedStandardDescriptors)
{
    for (int closedDescriptors = 1; closedDescriptors < 8; ++closedDescriptors) {
        for (const auto timeout : { 0, 2 }) {
            SCOPED_TRACE(closedDescriptors);
            SCOPED_TRACE(timeout);
            // Close descriptors only in a subprocess so GoogleTest keeps its own stdio.
            ASSERT_EXIT(
              {
                  for (int descriptor = STDIN_FILENO; descriptor <= STDERR_FILENO; ++descriptor) {
                      if (closedDescriptors & (1 << descriptor)) {
                          ::close(descriptor);
                      }
                  }
                  const std::string content = "payload through stdin\n";
                  const auto result = linglong::utils::Cmd("cat")
                                        .setTimeout(std::chrono::seconds(timeout))
                                        .toStdin(content)
                                        .exec();
                  ::_exit(result && *result == content ? EXIT_SUCCESS : EXIT_FAILURE);
              },
              ::testing::ExitedWithCode(EXIT_SUCCESS),
              "");
        }
    }
}

TEST(command, InheritsStderr)
{
    ASSERT_EXIT(
      {
          const auto result = linglong::utils::Cmd("sh").exec(
            { "-c", "printf stdout; printf 'inherited stderr\\n' >&2" });
          ::_exit(result && *result == "stdout" ? EXIT_SUCCESS : EXIT_FAILURE);
      },
      ::testing::ExitedWithCode(EXIT_SUCCESS),
      "inherited stderr");
}

TEST(command, TimeoutPreservesSuccessfulOutput)
{
    for (const auto timeout : { 1, 0, -1 }) {
        SCOPED_TRACE(timeout);
        const auto result = linglong::utils::Cmd("sh")
                              .setTimeout(std::chrono::seconds(timeout))
                              .exec({ "-c", "printf success" });
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_EQ(*result, "success");
    }
}

TEST(command, ExchangesLargeInputAndOutput)
{
    std::string content(256 * 1024 + 37, 'x');
    content.front() = '\0';
    content.back() = '\n';
    for (const auto timeout : { 0, 2 }) {
        const auto result = linglong::utils::Cmd("cat")
                              .setTimeout(std::chrono::seconds(timeout))
                              .toStdin(content)
                              .exec();
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_EQ(*result, content);
    }
}

TEST(command, TimeoutCoversStdoutAndProcessExitAndReapsChild)
{
    TempDir directory;
    const auto fifo = directory.path() / "blocked";
    const auto pidFile = directory.path() / "pid";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    for (const bool closeStdout : { false, true }) {
        SCOPED_TRACE(closeStdout);
        const auto startedAt = std::chrono::steady_clock::now();
        const std::string script = "echo $$ > \"$1\"; "
          + std::string(closeStdout ? "exec 1>&-; " : "") + "read value < \"$2\"";
        const auto result =
          linglong::utils::Cmd("sh")
            .setTimeout(std::chrono::milliseconds(200))
            .exec({ "-c", script, "timeout-test", pidFile.string(), fifo.string() });
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().message().find("command timed out after 200 ms:"),
                  std::string::npos);
        EXPECT_NE(result.error().message().find("/sh"), std::string::npos);
        const auto elapsed = std::chrono::steady_clock::now() - startedAt;
        EXPECT_GE(elapsed, std::chrono::milliseconds(200));
        EXPECT_LT(elapsed, std::chrono::seconds(3));
        pid_t child = -1;
        ASSERT_TRUE(std::ifstream(pidFile) >> child);
        ASSERT_GT(child, 0);
        EXPECT_EQ(::waitpid(child, nullptr, WNOHANG), -1);
        EXPECT_EQ(errno, ECHILD);
    }
}

TEST(command, TimeoutIncludesPartialOutput)
{
    TempDir directory;
    const auto fifo = directory.path() / "blocked";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    for (const bool closeStdout : { false, true }) {
        const std::string script = "printf diagnostic; "
          + std::string(closeStdout ? "exec 1>&-; " : "") + "read value < \"$1\"";
        const auto result = linglong::utils::Cmd("sh")
                              .setTimeout(std::chrono::milliseconds(200))
                              .exec({ "-c", script, "timeout-test", fifo.string() });
        ASSERT_FALSE(result);
        EXPECT_EQ(result.error().code(), ETIMEDOUT);
        EXPECT_NE(result.error().message().find("partial stdout: diagnostic"), std::string::npos);
    }
}

TEST(command, TimeoutLimitsPartialOutputInError)
{
    TempDir directory;
    const auto fifo = directory.path() / "blocked";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    const auto result = linglong::utils::Cmd("sh")
                          .setTimeout(std::chrono::milliseconds(200))
                          .exec({ "-c",
                                  "printf diagnostic; printf '%8192s' x; read value < \"$1\"",
                                  "timeout-test",
                                  fifo.string() });
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("partial stdout: diagnostic"), std::string::npos);
    EXPECT_NE(result.error().message().find("[truncated]"), std::string::npos);
    EXPECT_LT(result.error().message().size(), 5000U);
}

TEST(command, RepeatedTimeoutsDoNotLeakDescriptors)
{
    TempDir directory;
    const auto fifo = directory.path() / "blocked";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    ASSERT_TRUE(linglong::utils::Cmd("true").exec());
    const auto descriptorCount = [] {
        return std::distance(std::filesystem::directory_iterator("/proc/self/fd"),
                             std::filesystem::directory_iterator{});
    };
    const auto before = descriptorCount();
    for (int attempt = 0; attempt < 4; ++attempt) {
        for (const bool closeStdout : { false, true }) {
            const std::string script =
              std::string(closeStdout ? "exec 1>&-; " : "") + "read value < \"$1\"";
            const auto result = linglong::utils::Cmd("sh")
                                  .setTimeout(std::chrono::milliseconds(50))
                                  .exec({ "-c", script, "timeout-test", fifo.string() });
            ASSERT_FALSE(result);
            EXPECT_NE(result.error().message().find("timed out"), std::string::npos);
        }
    }
    EXPECT_EQ(descriptorCount(), before);
}

TEST(command, TimeoutSharesBudgetBetweenCommunicationAndProcessExit)
{
    const auto result = linglong::utils::Cmd("sh")
                          .setTimeout(std::chrono::milliseconds(500))
                          .exec({ "-c", "sleep 0.3; exec 1>&-; sleep 0.3" });
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("command timed out after 500 ms:"), std::string::npos);
}

TEST(command, TimeoutStopsContinuousStdout)
{
    const auto result = linglong::utils::Cmd("sh")
                          .setTimeout(std::chrono::milliseconds(100))
                          .exec({ "-c", "while :; do printf output; done" });
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("timed out"), std::string::npos);
}

TEST(command, TimeoutTerminatesDescendantHoldingStdout)
{
    TempDir directory;
    const auto fifo = directory.path() / "blocked";
    const auto pidFile = directory.path() / "descendant";
    ASSERT_EQ(::mkfifo(fifo.c_str(), 0600), 0);
    pid_t descendant = -1;
    const auto cleanup = linglong::utils::finally::finally([&] {
        if (descendant > 0) {
            ::kill(descendant, SIGKILL);
        }
    });
    const auto result = linglong::utils::Cmd("sh")
                          .setTimeout(std::chrono::milliseconds(200))
                          .exec({ "-c",
                                  "(read value < \"$1\") & echo $! > \"$2\"; exit 0",
                                  "timeout-test",
                                  fifo.string(),
                                  pidFile.string() });
    ASSERT_TRUE(std::ifstream(pidFile) >> descendant);
    ASSERT_GT(descendant, 0);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("timed out"), std::string::npos);
    // An orphan can briefly remain a zombie until its new parent reaps it.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    bool stopped = false;
    while (!stopped && std::chrono::steady_clock::now() < deadline) {
        std::ifstream status("/proc/" + std::to_string(descendant) + "/status");
        if (!status) {
            stopped = true;
        }
        std::string line;
        while (std::getline(status, line)) {
            if (line.compare(0, 6, "State:") == 0 && line.find('Z') != std::string::npos) {
                stopped = true;
            }
        }
        if (!stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    EXPECT_TRUE(stopped);
    if (stopped) {
        descendant = -1;
    }
}

} // namespace
