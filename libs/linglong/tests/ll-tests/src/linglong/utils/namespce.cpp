
// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/cmd.h"
#include "linglong/utils/namespace.h"

#include <sys/capability.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

TEST(NamespaceTest, GetUserNameWithUnknownUid)
{
    auto userName = linglong::utils::detail::getUserName(static_cast<uid_t>(-1));

    EXPECT_FALSE(userName.has_value());
    EXPECT_NE(userName.error().message().find("entry not found"), std::string::npos);
}

namespace {

bool canCreateUserNamespace()
{
    pid_t child = ::fork();
    if (child == -1) {
        return false;
    }
    if (child == 0) {
        ::_exit(::unshare(CLONE_NEWUSER | CLONE_NEWNS) == 0 ? 0 : 1);
    }

    int status = 0;
    while (::waitpid(child, &status, 0) == -1) {
        if (errno != EINTR) {
            return false;
        }
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool canAutoMapCurrentUser()
{
    if (!canCreateUserNamespace()) {
        return false;
    }
    if (!linglong::utils::Cmd{ "newuidmap" }.exists()
        || !linglong::utils::Cmd{ "newgidmap" }.exists()) {
        return false;
    }

    auto uidRanges = linglong::utils::detail::getSubuidRange(::geteuid(), true);
    auto gidRanges = linglong::utils::detail::getSubuidRange(::getegid(), false);
    return uidRanges && !uidRanges->empty() && gidRanges && !gidRanges->empty();
}

int runInNamespaceCommand(std::vector<std::string> arguments,
                          std::optional<uid_t> uid = std::nullopt,
                          std::optional<gid_t> gid = std::nullopt)
{
    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1);
    for (auto &argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    auto result =
      linglong::utils::runInNamespace(static_cast<int>(arguments.size()), argv.data(), uid, gid);
    EXPECT_TRUE(result.has_value()) << result.error().message();
    return result.value_or(-1);
}

} // namespace

TEST(NamespaceTest, GetSelfExeReturnsCurrentExecutable)
{
    auto self = linglong::utils::getSelfExe();

    ASSERT_TRUE(self.has_value()) << self.error().message();
    EXPECT_TRUE(std::filesystem::exists(*self));
    EXPECT_TRUE(std::filesystem::is_regular_file(*self));
}

TEST(NamespaceTest, NeedRunInNamespaceReportsCapabilityState)
{
    auto needed = linglong::utils::needRunInNamespace();

    ASSERT_TRUE(needed.has_value()) << needed.error().message();

    auto *caps = cap_get_proc();
    ASSERT_NE(caps, nullptr);
    cap_flag_value_t admin = CAP_CLEAR;
    ASSERT_EQ(cap_get_flag(caps, CAP_SYS_ADMIN, CAP_EFFECTIVE, &admin), 0);
    cap_free(caps);
    EXPECT_EQ(*needed, admin != CAP_SET);
}

TEST(NamespaceTest, RunInNamespaceReturnsChildExitCode)
{
    if (!canCreateUserNamespace()) {
        GTEST_SKIP() << "user and mount namespaces are not available";
    }
    EXPECT_EQ(runInNamespaceCommand({ "/bin/sh", "-c", "exit 7" }, geteuid(), getegid()), 7);
}

TEST(NamespaceTest, RunInNamespaceReturnsSignalExitCode)
{
    if (!canCreateUserNamespace()) {
        GTEST_SKIP() << "user and mount namespaces are not available";
    }
    EXPECT_EQ(runInNamespaceCommand({ "/bin/sh", "-c", "kill -TERM $$" }, geteuid(), getegid()),
              128 + SIGTERM);
}

TEST(NamespaceTest, RunInNamespaceReportsMissingExecutableAsFailure)
{
    if (!canCreateUserNamespace()) {
        GTEST_SKIP() << "user and mount namespaces are not available";
    }
    EXPECT_EQ(runInNamespaceCommand({ "/nonexistent/linglong-test-command" }, geteuid(), getegid()),
              255);
}

TEST(NamespaceTest, RunInNamespaceAutoMapsCurrentUserWhenIdsAreOmitted)
{
    if (!canAutoMapCurrentUser()) {
        GTEST_SKIP() << "newuidmap/newgidmap or subuid/subgid ranges are not available";
    }
    EXPECT_EQ(runInNamespaceCommand({ "/bin/sh", "-c", "exit 0" }), 0);
}

TEST(NamespaceTest, GetSubuidRangeUsesCurrentUser)
{
    if (!canAutoMapCurrentUser()) {
        GTEST_SKIP() << "newuidmap/newgidmap or subuid/subgid ranges are not available";
    }

    auto ranges = linglong::utils::detail::getSubuidRange(geteuid(), true);

    ASSERT_TRUE(ranges.has_value()) << ranges.error().message();
    ASSERT_FALSE(ranges->empty()) << "/etc/subuid has no range for the current user";
    for (const auto &range : *ranges) {
        EXPECT_FALSE(range.subuid.empty());
        EXPECT_FALSE(range.count.empty());
    }
}

TEST(NamespaceTest, GetSubuidRangeRejectsUnknownUser)
{
    auto ranges = linglong::utils::detail::getSubuidRange(static_cast<uid_t>(-1), false);

    EXPECT_FALSE(ranges.has_value());
    EXPECT_NE(ranges.error().message().find("failed to get user name"), std::string::npos);
}
