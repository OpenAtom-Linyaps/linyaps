/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/repo/config.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/yaml.h"
#include "ytj/ytj.hpp"

#include <fmt/format.h>

#include <fstream>

namespace linglong::repo {

utils::error::Result<api::types::v1::RepoConfigV2>
loadConfig(const std::filesystem::path &file) noexcept
{
    LINGLONG_TRACE(fmt::format("load repo config from {}", file));

    try {
        auto ifs = std::ifstream(file);
        if (!ifs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        // 尝试加载新版本配置
        auto config = utils::serialize::LoadYAML<api::types::v1::RepoConfigV2>(ifs);
        if (!config) {
            ifs.seekg(0);
            auto configV1 = utils::serialize::LoadYAML<api::types::v1::RepoConfig>(ifs);
            if (!configV1) {
                return LINGLONG_ERR("parse yaml failed");
            }

            // 将旧版本配置转换为新版本
            // A version-1 config is only valid when it names a repository that
            // is actually listed. Report the mismatch instead of dereferencing
            // the past-the-end iterator returned by std::find_if.
            auto converted = convertToV2(*configV1);
            if (!converted) {
                return LINGLONG_ERR("failed to convert config to v2", converted);
            }

            config = std::move(*converted);
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

utils::error::Result<api::types::v1::RepoConfigV2>
loadConfig(const std::vector<std::filesystem::path> &files) noexcept
{
    LINGLONG_TRACE("load repo config");

    for (const auto &file : files) {
        auto config = loadConfig(file);
        if (!config.has_value()) {
            LogD("Failed to load repo config from {}: {}", file, config.error());
            continue;
        }

        LogD("load repo config from {}", file);
        return config;
    }

    return LINGLONG_ERR("all failed");
}

utils::error::Result<void> saveConfig(const api::types::v1::RepoConfigV2 &cfg,
                                      const std::filesystem::path &path) noexcept
{
    LINGLONG_TRACE(fmt::format("save config to {}", path));

    try {
        auto defaultRepoExists =
          std::any_of(cfg.repos.begin(), cfg.repos.end(), [&cfg](const auto &repo) {
              return repo.alias.value_or(repo.name) == cfg.defaultRepo;
          });

        if (!defaultRepoExists) {
            return LINGLONG_ERR("default repo not found in repos");
        }

        auto ofs = std::ofstream(path);
        if (!ofs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto node = ytj::to_yaml(cfg);
        ofs << node;
        ofs.close();
        if (!ofs) {
            return LINGLONG_ERR("write failed");
        }

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

utils::error::Result<api::types::v1::Repo>
getDefaultRepo(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    LINGLONG_TRACE("get default repo");

    // A repository is selected by its alias when one is set, otherwise by its
    // name. Configs that reference a repository which is missing from "repos"
    // (including an empty "repos" list) used to dereference the past-the-end
    // iterator returned by std::find_if, which is undefined behaviour and
    // usually crashes. Treat that state as a normal error and let callers
    // decide how to surface it.
    auto defaultRepo = std::find_if(cfg.repos.begin(), cfg.repos.end(), [&cfg](const auto &repo) {
        return repo.alias.value_or(repo.name) == cfg.defaultRepo;
    });

    if (defaultRepo == cfg.repos.end()) {
        return LINGLONG_ERR(fmt::format("default repo {} not found in repos", cfg.defaultRepo));
    }

    return *defaultRepo;
}

std::vector<api::types::v1::Repo> getPrioritySortedRepos(api::types::v1::RepoConfigV2 cfg) noexcept
{
    std::stable_sort(cfg.repos.begin(), cfg.repos.end(), [](const auto &repo1, const auto &repo2) {
        return repo1.priority > repo2.priority;
    });
    return cfg.repos;
}

std::vector<std::vector<api::types::v1::Repo>>
getPriorityGroupedRepos(api::types::v1::RepoConfigV2 cfg) noexcept
{
    auto sortedRepos = getPrioritySortedRepos(std::move(cfg));
    if (sortedRepos.empty()) {
        return {};
    }

    std::vector<std::vector<api::types::v1::Repo>> groupedRepos;
    for (const auto &repo : sortedRepos) {
        if (groupedRepos.empty() || groupedRepos.back().front().priority != repo.priority) {
            groupedRepos.emplace_back();
        }
        groupedRepos.back().emplace_back(repo);
    }
    return groupedRepos;
}

utils::error::Result<api::types::v1::RepoConfigV2>
convertToV2(const api::types::v1::RepoConfig &cfg) noexcept
{
    LINGLONG_TRACE("convert repo config to v2");

    api::types::v1::RepoConfigV2 configV2;
    configV2.version = 2;
    configV2.defaultRepo = cfg.defaultRepo;
    int64_t priority = 0;

    // The default repository is promoted to the highest priority entry. A
    // legacy config that does not list its default repository is invalid, so
    // bail out with an error instead of dereferencing the past-the-end
    // iterator returned by std::find_if.
    auto defaultRepo = std::find_if(cfg.repos.begin(), cfg.repos.end(), [&cfg](const auto &repo) {
        return repo.first == cfg.defaultRepo;
    });

    if (defaultRepo == cfg.repos.end()) {
        return LINGLONG_ERR(fmt::format("default repo {} not found in repos", cfg.defaultRepo));
    }

    api::types::v1::Repo repoV2{
        .name = defaultRepo->first,
        .priority = priority,
        .url = defaultRepo->second,
    };

    configV2.repos.emplace_back(std::move(repoV2));
    priority -= 100;

    for (const auto &[name, url] : cfg.repos) {
        if (name == cfg.defaultRepo) {
            continue;
        }

        api::types::v1::Repo repoV2{ .name = name, .priority = priority, .url = url };
        configV2.repos.emplace_back(std::move(repoV2));
        priority -= 100;
    }

    return configV2;
}

utils::error::Result<int64_t> getRepoMinPriority(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    LINGLONG_TRACE("get minimum repo priority");

    // std::min_element returns end() for an empty range and dereferencing it
    // is undefined behaviour, so report configs without any repository instead.
    if (cfg.repos.empty()) {
        return LINGLONG_ERR("no repo found");
    }

    auto minElement = std::min_element(cfg.repos.begin(),
                                       cfg.repos.end(),
                                       [](const auto &repo1, const auto &repo2) {
                                           return repo1.priority < repo2.priority;
                                       });

    return minElement->priority;
}

utils::error::Result<int64_t> getRepoMaxPriority(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    LINGLONG_TRACE("get maximum repo priority");

    // See getRepoMinPriority: an empty "repos" list must not be dereferenced.
    if (cfg.repos.empty()) {
        return LINGLONG_ERR("no repo found");
    }

    auto maxElement = std::max_element(cfg.repos.begin(),
                                       cfg.repos.end(),
                                       [](const auto &repo1, const auto &repo2) {
                                           return repo1.priority < repo2.priority;
                                       });

    return maxElement->priority;
}

} // namespace linglong::repo
