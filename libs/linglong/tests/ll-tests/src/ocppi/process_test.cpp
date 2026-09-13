// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "ocppi/cli/Process.hpp"

#include <functional>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

namespace {

void expectRunProcessWaitsForItsOwnChild(const std::function<int()> &run)
{
    const auto unrelatedChild = ::fork();
    ASSERT_GE(unrelatedChild, 0);
    if (unrelatedChild == 0) {
        ::_exit(42);
    }

    siginfo_t childInfo{};
    ASSERT_EQ(::waitid(P_PID, unrelatedChild, &childInfo, WEXITED | WNOWAIT), 0);

    EXPECT_EQ(run(), 0);

    int status{};
    ASSERT_EQ(::waitpid(unrelatedChild, &status, 0), unrelatedChild);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 42);
}

TEST(OcppiProcess, WaitsForOwnChildWithoutOutput)
{
    expectRunProcessWaitsForItsOwnChild([]() {
        return runProcess("/bin/sh", { "-c", "sleep 0.1" });
    });
}

TEST(OcppiProcess, WaitsForOwnChildWithOutput)
{
    expectRunProcessWaitsForItsOwnChild([]() {
        std::string output;
        return runProcess("/bin/sh", { "-c", "exec 1>&-; sleep 0.1" }, output);
    });
}

} // namespace
