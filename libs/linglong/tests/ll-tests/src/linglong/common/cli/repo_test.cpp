/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/common/cli/repo.h"

namespace linglong::common::cli {
namespace {

using linglong::api::types::v1::Repo;
using linglong::api::types::v1::RepoConfigV2;

CLI::Validator validatorString{ [](const std::string &parameter) {
                                   if (parameter.empty()) {
                                       return std::string{ "empty" };
                                   }
                                   return std::string{};
                               },
                                "" };

class RepoCommandTest : public ::testing::Test
{
protected:
    RepoCommandTest()
        : repoApp(addRepoCommand(app, options, "repo", validatorString, "test-app"))
    {
    }

    void parse(const std::string &command) { app.parse(command); }

    utils::error::Result<void> handle()
    {
        return handleRepoCommand(
          repoApp,
          options,
          { .getConfig = [this]() -> utils::error::Result<RepoConfigV2> {
               return config;
           },
            .setConfig = [this](const RepoConfigV2 &cfg) -> utils::error::Result<void> {
                config = cfg;
                ++setConfigCalls;
                return LINGLONG_OK;
            } },
          { .showConfig = [this](const RepoConfigV2 &cfg) {
              shownConfig = cfg;
              ++showConfigCalls;
          } });
    }

    CLI::App app{ "test" };
    RepoOptions options;
    CLI::App *repoApp{ nullptr };
    RepoConfigV2 config{
        .defaultRepo = "stable",
        .repos = { Repo{ .alias = "stable",
                         .mirrorEnabled = false,
                         .name = "stable",
                         .priority = 10,
                         .url = "https://stable.example" },
                   Repo{ .alias = "beta",
                         .mirrorEnabled = true,
                         .name = "beta",
                         .priority = 20,
                         .url = "https://beta.example" } },
        .version = 2,
    };
    std::optional<RepoConfigV2> shownConfig;
    int setConfigCalls{ 0 };
    int showConfigCalls{ 0 };
};

TEST_F(RepoCommandTest, AddRepoTrimsTrailingSlash)
{
    parse("repo add community https://community.example/ --alias comm");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_EQ(setConfigCalls, 1);
    ASSERT_EQ(config.repos.size(), 3);
    EXPECT_EQ(config.repos.back().name, "community");
    ASSERT_TRUE(config.repos.back().alias.has_value());
    EXPECT_EQ(*config.repos.back().alias, "comm");
    EXPECT_EQ(config.repos.back().url, "https://community.example");
    EXPECT_EQ(config.repos.back().priority, 0);
}

TEST_F(RepoCommandTest, RejectDuplicateAlias)
{
    parse("repo add stable2 https://stable2.example --alias stable");

    auto ret = handle();

    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(setConfigCalls, 0);
    EXPECT_EQ(config.repos.size(), 2);
}

TEST_F(RepoCommandTest, UpdateRepoUrl)
{
    parse("repo update beta https://new-beta.example/");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_EQ(setConfigCalls, 1);
    EXPECT_EQ(config.repos[1].url, "https://new-beta.example");
}

TEST_F(RepoCommandTest, RemoveDefaultRepoSelectsHighestPriorityRemainingRepo)
{
    parse("repo remove stable");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_EQ(setConfigCalls, 1);
    ASSERT_EQ(config.repos.size(), 1);
    EXPECT_EQ(config.repos[0].alias.value_or(config.repos[0].name), "beta");
    EXPECT_EQ(config.defaultRepo, "beta");
}

TEST_F(RepoCommandTest, SetDefaultRaisesPriority)
{
    parse("repo set-default beta");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_EQ(setConfigCalls, 1);
    EXPECT_EQ(config.defaultRepo, "beta");
    EXPECT_EQ(config.repos[1].priority, 120);
}

TEST_F(RepoCommandTest, SetPriority)
{
    parse("repo set-priority stable 42");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_EQ(setConfigCalls, 1);
    EXPECT_EQ(config.repos[0].priority, 42);
}

TEST_F(RepoCommandTest, EnableMirror)
{
    parse("repo enable-mirror stable");
    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_TRUE(config.repos[0].mirrorEnabled.has_value());
    EXPECT_TRUE(*config.repos[0].mirrorEnabled);
}

TEST_F(RepoCommandTest, DisableMirror)
{
    parse("repo disable-mirror beta");
    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    ASSERT_TRUE(config.repos[1].mirrorEnabled.has_value());
    EXPECT_FALSE(*config.repos[1].mirrorEnabled);
}

TEST_F(RepoCommandTest, ShowUsesCallbackWithoutSaving)
{
    parse("repo show");

    auto ret = handle();

    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    EXPECT_EQ(showConfigCalls, 1);
    EXPECT_EQ(setConfigCalls, 0);
    ASSERT_TRUE(shownConfig.has_value());
    EXPECT_EQ(shownConfig->defaultRepo, "stable");
}

TEST_F(RepoCommandTest, ModifyReturnsError)
{
    parse("repo modify https://example.com");

    auto ret = handle();

    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(setConfigCalls, 0);
}

} // namespace
} // namespace linglong::common::cli
