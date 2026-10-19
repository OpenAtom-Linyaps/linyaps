
// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/error.h"
#include "linglong/utils/namespace.h"

#include <cstdlib>
#include <fstream>

#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils::detail {
// Internal procfs writer used by the namespace setup path.
utils::error::Result<void> writeMapFile(pid_t pid, bool isUid, const std::string &content);
utils::error::Result<void> denySetGroups(pid_t pid);
} // namespace linglong::utils::detail

TEST(NamespaceTest, RejectedUidMapWriteReturnsError)
{
    const auto path = "/proc/" + std::to_string(::getpid()) + "/uid_map";
    std::ofstream probe(path);
    if (!probe.is_open()) {
        GTEST_SKIP() << "Cannot open this process's uid_map for writing";
    }
    probe.close();

    // Invalid mapping text cannot change the process's ID mappings.
    const auto result = linglong::utils::detail::writeMapFile(::getpid(), true, "invalid-map\n");
    ASSERT_FALSE(result.has_value());
    const auto prefix = "failed to write " + path + ": ";
    const auto message = result.error().message();
    // Depending on the namespace's permissions, the kernel rejects either the
    // write itself (EPERM) or the invalid mapping text (EINVAL).
    EXPECT_TRUE(
      message.find(prefix + linglong::common::error::errorString(EPERM)) != std::string::npos
      || message.find(prefix + linglong::common::error::errorString(EINVAL)) != std::string::npos)
      << message;
}

TEST(NamespaceTest, RejectedGidMapWriteReturnsError)
{
    const auto path = "/proc/" + std::to_string(::getpid()) + "/gid_map";
    std::ofstream probe(path);
    if (!probe.is_open()) {
        GTEST_SKIP() << "Cannot open this process's gid_map for writing";
    }
    probe.close();

    const auto result = linglong::utils::detail::writeMapFile(::getpid(), false, "invalid-map\n");
    ASSERT_FALSE(result.has_value());
    const auto prefix = "failed to write " + path + ": ";
    const auto message = result.error().message();
    // Depending on the namespace's permissions, the kernel rejects either the
    // write itself (EPERM) or the invalid mapping text (EINVAL).
    EXPECT_TRUE(
      message.find(prefix + linglong::common::error::errorString(EPERM)) != std::string::npos
      || message.find(prefix + linglong::common::error::errorString(EINVAL)) != std::string::npos)
      << message;
}

TEST(NamespaceTest, RejectedSetGroupsWriteReturnsError)
{
    // Once a GID mapping exists, the kernel forbids changing setgroups.
    // Check this before writing so the test cannot change an unmapped namespace.
    std::ifstream mapping("/proc/self/gid_map");
    std::string entry;
    if (!std::getline(mapping, entry) || entry.empty()) {
        GTEST_SKIP() << "Requires an existing GID mapping";
    }

    const auto path = "/proc/" + std::to_string(::getpid()) + "/setgroups";
    std::ofstream probe(path);
    if (!probe.is_open()) {
        GTEST_SKIP() << "Cannot open this process's setgroups for writing";
    }
    probe.close();

    const auto result = linglong::utils::detail::denySetGroups(::getpid());
    ASSERT_FALSE(result.has_value());
    const auto expected =
      "failed to write " + path + ": " + linglong::common::error::errorString(EPERM);
    EXPECT_NE(result.error().message().find(expected), std::string::npos);
}

class NamespaceMappingFailureTest : public ::testing::TestWithParam<bool>
{
};

TEST_P(NamespaceMappingFailureTest, ReapsChildBeforeReturning)
{
    const auto probe = fork();
    ASSERT_NE(probe, -1);
    if (probe == 0) {
        _exit(unshare(CLONE_NEWUSER | CLONE_NEWNS) == 0 ? 0 : 1);
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(probe, &status, 0);
    } while (waited == -1 && errno == EINTR);
    ASSERT_EQ(waited, probe);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        GTEST_SKIP() << "User and mount namespaces are unavailable";
    }

    // Re-exec the test binary instead of inheriting threads and exit handlers
    // left by earlier tests. This also isolates waitpid(-1) from their children.
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    // The alarm bounds a broken cleanup handshake.
    // Exit normally after the library calls so coverage runtimes can flush the
    // counters collected in the isolated process.
    const bool failUid = GetParam();
    ASSERT_EXIT(([&]() {
                    alarm(5);
                    char command[] = "/bin/true";
                    char *argv[] = { command, nullptr };
                    const auto invalidID = static_cast<uid_t>(-1);
                    const auto result = linglong::utils::runInNamespace(1,
                                                                        argv,
                                                                        failUid ? invalidID : 0,
                                                                        failUid ? 0 : invalidID);
                    if (result
                        || result.error().message().find(failUid ? "failed to mapping uid"
                                                                 : "failed to mapping gid")
                          == std::string::npos) {
                        std::exit(1);
                    }
                    errno = 0;
                    const auto remaining = waitpid(-1, nullptr, WNOHANG);
                    if (remaining != -1 || errno != ECHILD) {
                        std::exit(2);
                    }
                    const auto success = linglong::utils::runInNamespace(1, argv, 0, 0);
                    std::exit(success && *success == 0 ? 0 : 3);
                }()),
                ::testing::ExitedWithCode(0),
                "");
}

INSTANTIATE_TEST_SUITE_P(UidAndGid, NamespaceMappingFailureTest, ::testing::Bool());

TEST(NamespaceTest, ParseSubuidRanges)
{
    uid_t uid = 0;
    // multiple ranges for the same user
    std::string content1 = "user1:100000:65536\n"
                           "user2:200000:65536\n"
                           "user1:300000:1000\n";
    std::stringstream ss1(content1);
    auto ranges1 = linglong::utils::detail::parseSubuidRanges(ss1, uid, "user1");
    ASSERT_TRUE(ranges1.has_value());
    ASSERT_EQ(ranges1->size(), 2);
    EXPECT_EQ((*ranges1)[0].subuid, "100000");
    EXPECT_EQ((*ranges1)[0].count, "65536");
    EXPECT_EQ((*ranges1)[1].subuid, "300000");
    EXPECT_EQ((*ranges1)[1].count, "1000");

    // user not exists
    std::stringstream ss2(content1);
    auto ranges2 = linglong::utils::detail::parseSubuidRanges(ss2, uid, "nonexistent");
    ASSERT_TRUE(ranges2.has_value());
    EXPECT_TRUE((*ranges2).empty());

    // missing field
    std::string content3 = "user1:100000\n";
    std::stringstream ss3(content3);
    auto ranges3 = linglong::utils::detail::parseSubuidRanges(ss3, uid, "user1");
    ASSERT_FALSE(ranges3.has_value());

    // empty input
    std::stringstream ss4("");
    auto ranges4 = linglong::utils::detail::parseSubuidRanges(ss4, uid, "user1");
    ASSERT_TRUE(ranges4.has_value());
    EXPECT_TRUE((*ranges4).empty());

    // username is prefix of another username
    std::string content5 = "user1_long:100000:65536\n"
                           "user1:200000:65536\n";
    std::stringstream ss5(content5);
    auto ranges5 = linglong::utils::detail::parseSubuidRanges(ss5, uid, "user1");
    ASSERT_TRUE(ranges5.has_value());
    ASSERT_EQ(ranges5->size(), 1);
    EXPECT_EQ((*ranges5)[0].subuid, "200000");
    EXPECT_EQ((*ranges5)[0].count, "65536");

    // use uid
    std::string content6 = "1000:100000:65536\n"
                           "user1:200000:65536\n";
    std::stringstream ss6(content6);
    auto ranges6 = linglong::utils::detail::parseSubuidRanges(ss6, 1000, "user1");
    ASSERT_TRUE(ranges6.has_value());
    ASSERT_EQ(ranges6->size(), 2);
    EXPECT_EQ((*ranges6)[0].subuid, "100000");
    EXPECT_EQ((*ranges6)[0].count, "65536");
    EXPECT_EQ((*ranges6)[1].subuid, "200000");
    EXPECT_EQ((*ranges6)[1].count, "65536");
}
