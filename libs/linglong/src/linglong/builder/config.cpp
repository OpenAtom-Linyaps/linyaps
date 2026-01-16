/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/config.h"

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/common/dir.h"
#include "linglong/common/xdg.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"
#include "linglong/utils/serialize/yaml.h"

#include <fmt/format.h>

#include <fstream>

namespace linglong::builder {

utils::error::Result<api::types::v1::BuilderConfig>
initDefaultBuildConfig(const std::filesystem::path &path)
{
    LINGLONG_TRACE("init default build config");

    auto cacheLocation = common::xdg::getXDGCacheHomeDir();
    if (cacheLocation.empty()) {
        return LINGLONG_ERR("failed to get cache dir, neither XDG_CACHE_HOME nor HOME is set");
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create config dir", ec);
    }

    linglong::api::types::v1::BuilderConfig config;
    config.version = 1;
    config.repo = cacheLocation / "linglong-builder";
    auto ret = linglong::builder::saveConfig(config, path);
    if (!ret) {
        return LINGLONG_ERR("failed to save default build config file", ret.error());
    }

    return config;
}

auto loadConfig(const std::filesystem::path &path) noexcept
  -> utils::error::Result<api::types::v1::BuilderConfig>
{
    LINGLONG_TRACE(fmt::format("load build config from {}", path));

    try {
        auto config = utils::serialize::LoadYAMLFile<api::types::v1::BuilderConfig>(path);
        if (!config) {
            return LINGLONG_ERR("parse build config", config);
        }
        if (config->version != 1) {
            return LINGLONG_ERR(
              fmt::format("wrong configuration file version {}", config->version));
        }

        return config;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(e);
    }
}

auto loadConfig() noexcept -> utils::error::Result<api::types::v1::BuilderConfig>
{
    LINGLONG_TRACE("load build config");

    std::filesystem::path builderConfigPath = "builder/config.yaml";
    auto configDir = common::dir::getUserRuntimeConfigDir();
    if (configDir.empty()) {
        return LINGLONG_ERR("failed to get config dir");
    }

    std::error_code ec;
    auto path = configDir / builderConfigPath;
    if (!std::filesystem::exists(path, ec)) {
        if (ec) {
            return LINGLONG_ERR(fmt::format("failed to check build config file {}", path), ec);
        }

        return initDefaultBuildConfig(path);
    }

    auto config = loadConfig(path);
    if (!config) {
        return LINGLONG_ERR(config);
    }

    LogD("Load build config from {}", path);
    return config;
}

auto saveConfig(const api::types::v1::BuilderConfig &cfg,
                const std::filesystem::path &path) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE(fmt::format("save config to {}", path));

    try {
        auto ofs = std::ofstream(path);
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

} // namespace linglong::builder
