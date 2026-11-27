/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "ocppi/cli/Process.hpp"

#include <cerrno>
#include <cstring>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {

// Fork a child that exits immediately with the given code and is never
// reaped, so its zombie exit status is available to ::wait() and can be
// mistaken for the process spawned by runProcess (issue #1846).
pid_t spawnExitedDistractor(int exitCode)
{
    pid_t pid = ::fork();
    if (pid == 0) {
        ::_exit(exitCode);
    }
    return pid;
}

void reapQuietly(pid_t pid)
{
    if (pid > 0) {
        ::waitpid(pid, nullptr, 0); // may fail with ECHILD if already reaped
    }
}

} // namespace

TEST(ProcessTest, WaitsForSpawnedChildWithOutput)
{
    pid_t distractor = spawnExitedDistractor(7);
    ASSERT_NE(distractor, -1) << std::strerror(errno);

    std::string output;
    int status = runProcess("/bin/sh", { "-c", "echo hello; exit 42" }, output);

    // runProcess must report the status of the process it spawned (42),
    // not that of an unrelated terminated child (7).
    EXPECT_TRUE(WIFEXITED(status)) << "raw status: " << status;
    EXPECT_EQ(WEXITSTATUS(status), 42) << "raw status: " << status;
    EXPECT_EQ(output, "hello\n");

    reapQuietly(distractor);
}

TEST(ProcessTest, WaitsForSpawnedChildWithoutOutput)
{
    pid_t distractor = spawnExitedDistractor(7);
    ASSERT_NE(distractor, -1) << std::strerror(errno);

    int status = runProcess("/bin/sh", { "-c", "exit 42" });

    EXPECT_TRUE(WIFEXITED(status)) << "raw status: " << status;
    EXPECT_EQ(WEXITSTATUS(status), 42) << "raw status: " << status;

    reapQuietly(distractor);
}
