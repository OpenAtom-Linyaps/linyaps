/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/repo/config.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/yaml.h"
#include "ytj/ytj.hpp"

#include <fstream>

namespace linglong::repo {

utils::error::Result<api::types::v1::RepoConfigV2> loadConfig(const QString &file) noexcept
{
    LINGLONG_TRACE(QString("load repo config from %1").arg(file));

    try {
        auto ifs = std::ifstream(file.toLocal8Bit());
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
            config = convertToV2(*configV1);
        }

        auto repoExists =
          std::any_of(config->repos.begin(), config->repos.end(), [&config](const auto &repo) {
              return repo.alias.value_or(repo.name) == config->defaultRepo;
          });

        if (!repoExists) {
            return LINGLONG_ERR(QString("default repo not found in repos"));
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

utils::error::Result<api::types::v1::RepoConfigV2> loadConfig(const QStringList &files) noexcept
{
    LINGLONG_TRACE(QString("load repo config from %1").arg(files.join(" ")));

    for (const auto &file : files) {
        auto config = loadConfig(file);
        if (!config.has_value()) {
            qDebug() << "Failed to load repo config from" << file << ":" << config.error();
            continue;
        }

        qDebug() << "load repo config from" << file;
        return config;
    }

    return LINGLONG_ERR("all failed");
}

utils::error::Result<void> saveConfig(const api::types::v1::RepoConfigV2 &cfg,
                                      const QString &path) noexcept
{
    LINGLONG_TRACE(QString("save config to %1").arg(path));

    try {
        auto defaultRepoExists =
          std::any_of(cfg.repos.begin(), cfg.repos.end(), [&cfg](const auto &repo) {
              return repo.alias.value_or(repo.name) == cfg.defaultRepo;
          });

        if (!defaultRepoExists) {
            return LINGLONG_ERR("default repo not found in repos");
        }

        auto ofs = std::ofstream(path.toLocal8Bit());
        if (!ofs.is_open()) {
            return LINGLONG_ERR("open failed");
        }

        auto node = ytj::to_yaml(cfg);
        ofs << node;

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

api::types::v1::Repo getDefaultRepo(const api::types::v1::RepoConfigV2 &cfg) noexcept
{
    const auto &defaultRepo =
      std::find_if(cfg.repos.begin(), cfg.repos.end(), [&cfg](const auto &repo) {
          return repo.alias.value_or(repo.name) == cfg.defaultRepo;
      });

    return *defaultRepo;
}

api::types::v1::RepoConfigV2 convertToV2(const api::types::v1::RepoConfig &cfg) noexcept
{
    api::types::v1::RepoConfigV2 configV2;
    configV2.version = 2;
    configV2.defaultRepo = cfg.defaultRepo;

    for (const auto &[name, url] : cfg.repos) {
        api::types::v1::Repo repoV2;
        repoV2.name = name;
        repoV2.url = url;
        configV2.repos.push_back(repoV2);
    }

    return configV2;
}

} // namespace linglong::repo
