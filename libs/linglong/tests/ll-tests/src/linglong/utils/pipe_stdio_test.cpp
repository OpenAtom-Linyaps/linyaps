// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/io/pipe.h"

#include <csignal>

#include <sys/resource.h>
#include <sys/wait.h>

namespace {

using linglong::utils::io::PipeSet;

class ChildStdioTest : public ::testing::TestWithParam<int>
{
};

TEST_P(ChildStdioTest, TransfersAllChannelsWithInitiallyClosedStandardDescriptors)
{
    const int mask = GetParam();
    const auto child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        ::alarm(5);
        for (int fd = 0; fd < 3; ++fd) {
            if (mask & (1 << fd)) {
                ::close(fd);
            }
        }
        auto pipes = PipeSet::create();
        if (!pipes) {
            ::_exit(2);
        }
        const auto output = ::fcntl(pipes->stdoutPipe->readEnd(), F_DUPFD_CLOEXEC, 3);
        const auto error = ::fcntl(pipes->stderrPipe->readEnd(), F_DUPFD_CLOEXEC, 3);
        if (output < 0 || error < 0 || ::write(pipes->stdinPipe->writeEnd(), "i", 1) != 1) {
            ::_exit(3);
        }
        if (!pipes->setupChildStdio()) {
            ::_exit(4);
        }
        for (int fd = 0; fd < 3; ++fd) {
            const auto flags = ::fcntl(fd, F_GETFD);
            if (flags < 0 || (flags & FD_CLOEXEC)) {
                ::_exit(5);
            }
        }
        char value = 0;
        if (::read(STDIN_FILENO, &value, 1) != 1 || value != 'i') {
            ::_exit(6);
        }
        if (::write(STDOUT_FILENO, "o", 1) != 1 || ::write(STDERR_FILENO, "e", 1) != 1) {
            ::_exit(7);
        }
        if (::read(output, &value, 1) != 1 || value != 'o' || ::read(error, &value, 1) != 1
            || value != 'e') {
            ::_exit(8);
        }
        // Exercise exec as well as flags: the shell must still see all stdio.
        ::execl("/bin/sh",
                "sh",
                "-c",
                "test -p /dev/fd/0 && test -p /dev/fd/1 && test -p /dev/fd/2",
                static_cast<char *>(nullptr));
        ::_exit(9);
    }
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0) << "closed descriptor mask=" << mask;
}

INSTANTIATE_TEST_SUITE_P(ClosedDescriptorMasks, ChildStdioTest, ::testing::Range(0, 8));

TEST(ChildStdioSetupTest, ReportsInvalidSourceWithoutChangingOtherPipeEnds)
{
    const auto child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        ::alarm(5);
        auto pipes = PipeSet::create();
        if (!pipes) {
            ::_exit(2);
        }
        pipes->stdinPipe->closeReadEnd();
        if (pipes->setupChildStdio()) {
            ::_exit(3);
        }
        ::_exit(::fcntl(pipes->stdoutPipe->writeEnd(), F_GETFD) < 0
                    || ::fcntl(pipes->stderrPipe->writeEnd(), F_GETFD) < 0
                  ? 4
                  : 0);
    }
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(ChildStdioSetupTest, ReportsDescriptorLimitFailureBeforeRedirection)
{
    const auto child = ::fork();
    ASSERT_GE(child, 0);
    if (child == 0) {
        ::alarm(5);
        auto pipes = PipeSet::create();
        if (!pipes) {
            ::_exit(2);
        }
        struct rlimit limits{};
        if (::getrlimit(RLIMIT_NOFILE, &limits) != 0) {
            ::_exit(3);
        }
        limits.rlim_cur = 3;
        if (::setrlimit(RLIMIT_NOFILE, &limits) != 0) {
            ::_exit(4);
        }
        ::_exit(pipes->setupChildStdio() ? 1 : 0);
    }
    int status = 0;
    ASSERT_EQ(::waitpid(child, &status, 0), child);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

} // namespace
