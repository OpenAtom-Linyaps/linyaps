/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/api/types/v1/Repo.hpp"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/api/types/v1/RepoConfigV2.hpp"
#include "linglong/repo/config.h"

#include <common/tempdir.h>

#include <filesystem>
#include <fstream>

using namespace linglong::repo;
using namespace linglong::api::types::v1;

namespace fs = std::filesystem;

TEST(Repo, GetRepoMinPriority)
{
    RepoConfigV2 cfg;

    cfg.repos = { { std::nullopt, false, "repo1", 100, "http://example.com/repo1" } };
    auto minPriority = getRepoMinPriority(cfg);
    ASSERT_TRUE(minPriority.has_value()) << minPriority.error().message();
    EXPECT_EQ(*minPriority, 100);

    cfg.repos = { { std::nullopt, false, "repo1", 200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    minPriority = getRepoMinPriority(cfg);
    ASSERT_TRUE(minPriority.has_value()) << minPriority.error().message();
    EXPECT_EQ(*minPriority, 100);

    cfg.repos = { { std::nullopt, false, "repo1", -100, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 0, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 500, "http://example.com/repo3" } };
    minPriority = getRepoMinPriority(cfg);
    ASSERT_TRUE(minPriority.has_value()) << minPriority.error().message();
    EXPECT_EQ(*minPriority, -100);

    cfg.repos = { { "alias1", false, "repo1", 200, "http://example.com/repo1" },
                  { "alias2", false, "repo2", 100, "http://example.com/repo2" },
                  { "alias3", false, "repo3", 300, "http://example.com/repo3" } };
    minPriority = getRepoMinPriority(cfg);
    ASSERT_TRUE(minPriority.has_value()) << minPriority.error().message();
    EXPECT_EQ(*minPriority, 100);
}

TEST(Repo, GetRepoMaxPriority)
{
    RepoConfigV2 cfg;

    cfg.repos = { { std::nullopt, false, "repo1", 100, "http://example.com/repo1" } };
    auto maxPriority = getRepoMaxPriority(cfg);
    ASSERT_TRUE(maxPriority.has_value()) << maxPriority.error().message();
    EXPECT_EQ(*maxPriority, 100);

    cfg.repos = { { std::nullopt, false, "repo1", 200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    maxPriority = getRepoMaxPriority(cfg);
    ASSERT_TRUE(maxPriority.has_value()) << maxPriority.error().message();
    EXPECT_EQ(*maxPriority, 300);

    cfg.repos = { { std::nullopt, false, "repo1", -200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 0, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    maxPriority = getRepoMaxPriority(cfg);
    ASSERT_TRUE(maxPriority.has_value()) << maxPriority.error().message();
    EXPECT_EQ(*maxPriority, 300);

    cfg.repos = { { "alias1", false, "repo1", 200, "http://example.com/repo1" },
                  { "alias2", false, "repo2", 0, "http://example.com/repo2" },
                  { "alias3", false, "repo3", 300, "http://example.com/repo3" } };
    maxPriority = getRepoMaxPriority(cfg);
    ASSERT_TRUE(maxPriority.has_value()) << maxPriority.error().message();
    EXPECT_EQ(*maxPriority, 300);
}

TEST(Repo, ConventToV2)
{
    RepoConfig cfg;
    cfg.defaultRepo = "repo1";
    cfg.repos = { { "repo1", "http://example.com/repo1" } };

    auto configV2 = convertToV2(cfg);
    ASSERT_TRUE(configV2.has_value()) << configV2.error().message();

    EXPECT_EQ(configV2->defaultRepo, "repo1");
    EXPECT_EQ(configV2->repos.size(), 1);
    EXPECT_EQ(configV2->repos[0].name, "repo1");
    EXPECT_EQ(configV2->repos[0].url, "http://example.com/repo1");
    EXPECT_EQ(configV2->repos[0].priority, 0);

    cfg.defaultRepo = "repo2";
    cfg.repos = { { "repo1", "http://example.com/repo1" },
                  { "repo2", "http://example.com/repo2" } };
    configV2 = convertToV2(cfg);
    ASSERT_TRUE(configV2.has_value()) << configV2.error().message();

    EXPECT_EQ(configV2->defaultRepo, "repo2");
    EXPECT_EQ(configV2->repos.size(), 2);
    EXPECT_EQ(configV2->repos[0].name, "repo2");
    EXPECT_EQ(configV2->repos[0].url, "http://example.com/repo2");
    EXPECT_EQ(configV2->repos[0].priority, 0);
    EXPECT_EQ(configV2->repos[1].name, "repo1");
    EXPECT_EQ(configV2->repos[1].url, "http://example.com/repo1");
    EXPECT_EQ(configV2->repos[1].priority, -100);
}

TEST(Repo, GetPrioritySortedRepos)
{
    RepoConfigV2 cfg;
    EXPECT_TRUE(getPrioritySortedRepos(cfg).empty());

    cfg.repos = { { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" },
                  { std::nullopt, false, "repo1", 200, "http://example.com/repo1" } };

    auto sortedRepos = getPrioritySortedRepos(cfg);
    ASSERT_EQ(sortedRepos.size(), 3);
    EXPECT_EQ(sortedRepos[0].name, "repo3");
    EXPECT_EQ(sortedRepos[0].priority, 300);
    EXPECT_EQ(sortedRepos[1].name, "repo1");
    EXPECT_EQ(sortedRepos[1].priority, 200);
    EXPECT_EQ(sortedRepos[2].name, "repo2");
    EXPECT_EQ(sortedRepos[2].priority, 100);
}

TEST(Repo, GetPriorityGroupedRepos)
{
    RepoConfigV2 cfg;
    EXPECT_TRUE(getPriorityGroupedRepos(cfg).empty());

    cfg.repos = { { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo4", 200, "http://example.com/repo4" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" },
                  { std::nullopt, false, "repo1", 200, "http://example.com/repo1" } };

    auto groupedRepos = getPriorityGroupedRepos(cfg);
    ASSERT_EQ(groupedRepos.size(), 3);

    ASSERT_EQ(groupedRepos[0].size(), 1);
    EXPECT_EQ(groupedRepos[0][0].name, "repo3");
    EXPECT_EQ(groupedRepos[0][0].priority, 300);

    ASSERT_EQ(groupedRepos[1].size(), 2);
    EXPECT_EQ(groupedRepos[1][0].name, "repo4");
    EXPECT_EQ(groupedRepos[1][0].priority, 200);
    EXPECT_EQ(groupedRepos[1][1].name, "repo1");
    EXPECT_EQ(groupedRepos[1][1].priority, 200);

    ASSERT_EQ(groupedRepos[2].size(), 1);
    EXPECT_EQ(groupedRepos[2][0].name, "repo2");
    EXPECT_EQ(groupedRepos[2][0].priority, 100);
}

TEST(Repo, LoadConfigFromV2Yaml)
{
    TempDir dir;
    auto file = dir.path() / "config.yaml";
    std::ofstream(file) << R"(
defaultRepo: stable
repos:
  - name: stable
    priority: 0
    url: https://example.com/repo
version: 2
)";

    auto cfg = loadConfig(file);
    ASSERT_TRUE(cfg.has_value()) << cfg.error().message();
    EXPECT_EQ(cfg->defaultRepo, "stable");
    ASSERT_EQ(cfg->repos.size(), 1);
    EXPECT_EQ(cfg->repos[0].url, "https://example.com/repo");
}

TEST(Repo, LoadConfigMissingFileFails)
{
    TempDir dir;
    auto cfg = loadConfig(dir.path() / "nope.yaml");
    EXPECT_FALSE(cfg.has_value());
}

TEST(Repo, LoadConfigFromVectorTriesUntilSuccess)
{
    TempDir dir;
    auto bad = dir.path() / "bad.yaml";
    auto good = dir.path() / "good.yaml";
    std::ofstream(good) << R"(
defaultRepo: stable
repos:
  - name: stable
    priority: 0
    url: https://example.com/repo
version: 2
)";

    auto cfg = loadConfig(std::vector<fs::path>{ bad, good });
    ASSERT_TRUE(cfg.has_value());
    EXPECT_EQ(cfg->repos[0].url, "https://example.com/repo");
}

TEST(Repo, LoadConfigVectorAllFail)
{
    TempDir dir;
    auto cfg = loadConfig(std::vector<fs::path>{ dir.path() / "a.yaml", dir.path() / "b.yaml" });
    EXPECT_FALSE(cfg.has_value());
}

TEST(Repo, SaveConfigWritesFile)
{
    TempDir dir;
    auto cfg = RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };

    auto file = dir.path() / "saved.yaml";
    ASSERT_TRUE(saveConfig(cfg, file).has_value());
    EXPECT_TRUE(fs::exists(file));

    auto loaded = loadConfig(file);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->repos[0].url, "https://example.com/repo");
}

TEST(Repo, SaveConfigMissingDefaultRepoFails)
{
    TempDir dir;
    auto cfg = RepoConfigV2{
        .defaultRepo = "missing",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };
    auto file = dir.path() / "saved.yaml";
    EXPECT_FALSE(saveConfig(cfg, file).has_value());
}

TEST(Repo, GetDefaultRepoReturnsMatchingRepo)
{
    RepoConfigV2 cfg{
        .defaultRepo = "stable",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };

    auto repo = getDefaultRepo(cfg);

    ASSERT_TRUE(repo.has_value()) << repo.error().message();
    EXPECT_EQ(repo->name, "stable");
    EXPECT_EQ(repo->url, "https://example.com/repo");
    EXPECT_EQ(repo->priority, 0);
}

TEST(Repo, GetDefaultRepoMatchesRepoAlias)
{
    RepoConfigV2 cfg{
        .defaultRepo = "mirror",
        .repos = { Repo{ .alias = "mirror",
                         .name = "stable",
                         .priority = 0,
                         .url = "https://example.com/repo" } },
        .version = 2,
    };

    auto repo = getDefaultRepo(cfg);

    ASSERT_TRUE(repo.has_value()) << repo.error().message();
    EXPECT_EQ(repo->name, "stable");
    ASSERT_TRUE(repo->alias.has_value());
    EXPECT_EQ(*repo->alias, "mirror");
}

TEST(Repo, GetDefaultRepoMissingReturnsError)
{
    // Mirrors SaveConfigMissingDefaultRepoFails: this configuration must be
    // rejected instead of dereferencing the past-the-end iterator.
    RepoConfigV2 cfg{
        .defaultRepo = "missing",
        .repos = { Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" } },
        .version = 2,
    };

    auto repo = getDefaultRepo(cfg);

    EXPECT_FALSE(repo.has_value());
}

TEST(Repo, GetDefaultRepoEmptyReposReturnsError)
{
    RepoConfigV2 cfg{ .defaultRepo = "stable", .repos = {}, .version = 2 };

    auto repo = getDefaultRepo(cfg);

    EXPECT_FALSE(repo.has_value());
}

TEST(Repo, PriorityHelpersRejectEmptyRepos)
{
    // std::min_element/std::max_element return end() for an empty range, so
    // both helpers have to report that there is no repository to inspect.
    RepoConfigV2 cfg{ .defaultRepo = "", .repos = {}, .version = 2 };

    EXPECT_FALSE(getRepoMinPriority(cfg).has_value());
    EXPECT_FALSE(getRepoMaxPriority(cfg).has_value());
}

TEST(Repo, ConvertToV2MissingDefaultRepoFails)
{
    RepoConfig cfg;
    cfg.defaultRepo = "missing";
    cfg.repos = { { "stable", "https://example.com/repo" } };

    auto configV2 = convertToV2(cfg);

    EXPECT_FALSE(configV2.has_value());
}

TEST(Repo, ConvertToV2EmptyReposFails)
{
    RepoConfig cfg;
    cfg.defaultRepo = "stable";
    cfg.repos = {};

    auto configV2 = convertToV2(cfg);

    EXPECT_FALSE(configV2.has_value());
}

TEST(Repo, LoadConfigFromV1YamlWithMissingDefaultRepoFails)
{
    TempDir dir;
    auto file = dir.path() / "config.yaml";
    std::ofstream(file) << R"(
defaultRepo: missing
repos:
  stable: https://example.com/repo
version: 1
)";

    // Loading a version-1 config whose default repository is not listed must
    // fail cleanly instead of crashing through convertToV2.
    auto cfg = loadConfig(file);

    EXPECT_FALSE(cfg.has_value());
}
