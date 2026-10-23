// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/cmd.h"

#include <filesystem>
#include <string>

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

TEST(command, toStdinBrokenPipeYieldsFailure)
{
    // A child that exits without draining stdin used to kill the whole process
    // through SIGPIPE as soon as the payload exceeded the pipe buffer. It has to
    // be reported as a normal failure instead, because callers such as the
    // executable OCI config patches treat a failing command as a skippable
    // item. The default SIGPIPE disposition would have terminated the test
    // binary itself, so reaching the assertions below is already part of the
    // regression.
    const std::string payload(1 << 20, 'A');
    auto ret = linglong::utils::Cmd("sh").toStdin(payload).exec({ "-c", "exit 0" });

    ASSERT_FALSE(ret.has_value()) << "a child that never reads stdin must not report success";
    EXPECT_TRUE(ret.error().message().find("broken pipe") != std::string::npos)
      << "unexpected error message: " << ret.error().message();

    // The utility must stay usable right after a broken pipe.
    auto after = linglong::utils::Cmd("cat").toStdin("still alive").exec();
    ASSERT_TRUE(after.has_value()) << after.error().message();
    EXPECT_EQ(*after, "still alive");
}

TEST(command, toStdinBrokenPipeKeepsExitCodeAndOutput)
{
    // Even when the input cannot be handed over completely, the exit code and
    // whatever output the child produced must still be part of the error.
    const std::string payload(1 << 20, 'B');
    auto ret = linglong::utils::Cmd("sh").toStdin(payload).exec({ "-c", "echo partial; exit 7" });

    ASSERT_FALSE(ret.has_value());
    const auto message = ret.error().message();
    EXPECT_TRUE(message.find("broken pipe") != std::string::npos) << message;
    EXPECT_TRUE(message.find("exit code 7") != std::string::npos) << message;
    EXPECT_TRUE(message.find("partial") != std::string::npos) << message;
}

TEST(command, toStdinWithoutReaderIsRepeatable)
{
    // Running the same failing command several times must give the same result:
    // the write end closed by the loop must not be closed a second time by the
    // scope guard once another thread recycled the descriptor number.
    const std::string payload(1 << 20, 'C');
    for (int i = 0; i < 3; ++i) {
        auto ret = linglong::utils::Cmd("sh").toStdin(payload).exec({ "-c", "exit 0" });
        ASSERT_FALSE(ret.has_value()) << "iteration " << i;
        EXPECT_TRUE(ret.error().message().find("broken pipe") != std::string::npos)
          << "iteration " << i << ": " << ret.error().message();
    }
}

TEST(command, ExecKeepsFileDescriptorsStable)
{
    const auto countOpenFds = []() -> std::size_t {
        std::size_t count = 0;
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator("/proc/self/fd", ec)) {
            static_cast<void>(entry);
            ++count;
        }
        EXPECT_FALSE(ec) << "cannot inspect /proc/self/fd: " << ec.message();
        return count;
    };

    // Warm up first: the very first exec() may create descriptors that are kept
    // around (locale or resolver state), which would distort the baseline.
    ASSERT_TRUE(linglong::utils::Cmd("cat").toStdin("warmup").exec().has_value());

    const auto baseline = countOpenFds();
    for (int i = 0; i < 16; ++i) {
        // ordinary round trip
        auto drained = linglong::utils::Cmd("cat").toStdin(std::string(8192, 'x')).exec();
        ASSERT_TRUE(drained.has_value()) << drained.error().message();

        // child that stops reading: exercises the in-loop close of the write end
        auto broken =
          linglong::utils::Cmd("sh").toStdin(std::string(1 << 20, 'y')).exec({ "-c", "exit 0" });
        ASSERT_FALSE(broken.has_value());

        // no input at all: the write end is closed before the loop even starts
        auto empty = linglong::utils::Cmd("cat").toStdin("").exec();
        ASSERT_TRUE(empty.has_value()) << empty.error().message();
    }

    EXPECT_EQ(baseline, countOpenFds());
}

} // namespace
