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

using namespace linglong::repo;
using namespace linglong::api::types::v1;

TEST(Repo, GetRepoMinPriority)
{
    RepoConfigV2 cfg;

    cfg.repos = { { std::nullopt, false, "repo1", 100, "http://example.com/repo1" } };
    EXPECT_EQ(getRepoMinPriority(cfg), 100);

    cfg.repos = { { std::nullopt, false, "repo1", 200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMinPriority(cfg), 100);

    cfg.repos = { { std::nullopt, false, "repo1", -100, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 0, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 500, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMinPriority(cfg), -100);

    cfg.repos = { { "alias1", false, "repo1", 200, "http://example.com/repo1" },
                  { "alias2", false, "repo2", 100, "http://example.com/repo2" },
                  { "alias3", false, "repo3", 300, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMinPriority(cfg), 100);
}

TEST(Repo, GetRepoMaxPriority)
{
    RepoConfigV2 cfg;

    cfg.repos = { { std::nullopt, false, "repo1", 100, "http://example.com/repo1" } };
    EXPECT_EQ(getRepoMaxPriority(cfg), 100);

    cfg.repos = { { std::nullopt, false, "repo1", 200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 100, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMaxPriority(cfg), 300);

    cfg.repos = { { std::nullopt, false, "repo1", -200, "http://example.com/repo1" },
                  { std::nullopt, false, "repo2", 0, "http://example.com/repo2" },
                  { std::nullopt, false, "repo3", 300, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMaxPriority(cfg), 300);

    cfg.repos = { { "alias1", false, "repo1", 200, "http://example.com/repo1" },
                  { "alias2", false, "repo2", 0, "http://example.com/repo2" },
                  { "alias3", false, "repo3", 300, "http://example.com/repo3" } };
    EXPECT_EQ(getRepoMaxPriority(cfg), 300);
}

TEST(Repo, ConventToV2)
{
    RepoConfig cfg;
    cfg.defaultRepo = "repo1";
    cfg.repos = { { "repo1", "http://example.com/repo1" } };

    auto configV2 = convertToV2(cfg);

    EXPECT_EQ(configV2.defaultRepo, "repo1");
    EXPECT_EQ(configV2.repos.size(), 1);
    EXPECT_EQ(configV2.repos[0].name, "repo1");
    EXPECT_EQ(configV2.repos[0].url, "http://example.com/repo1");
    EXPECT_EQ(configV2.repos[0].priority, 0);

    cfg.defaultRepo = "repo2";
    cfg.repos = { { "repo1", "http://example.com/repo1" },
                  { "repo2", "http://example.com/repo2" } };
    configV2 = convertToV2(cfg);

    EXPECT_EQ(configV2.defaultRepo, "repo2");
    EXPECT_EQ(configV2.repos.size(), 2);
    EXPECT_EQ(configV2.repos[0].name, "repo2");
    EXPECT_EQ(configV2.repos[0].url, "http://example.com/repo2");
    EXPECT_EQ(configV2.repos[0].priority, 0);
    EXPECT_EQ(configV2.repos[1].name, "repo1");
    EXPECT_EQ(configV2.repos[1].url, "http://example.com/repo1");
    EXPECT_EQ(configV2.repos[1].priority, -100);
}
