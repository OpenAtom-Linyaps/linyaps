/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "runtime_config.h"

#include "linglong/common/dir.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"

#include <filesystem>
#include <fstream>

using RuntimeConfigure = linglong::api::types::v1::RuntimeConfigure;

namespace {

linglong::utils::error::Result<std::optional<RuntimeConfigure>>
loadUserRuntimeConfig(const std::string &appId)
{
    LINGLONG_TRACE(
      fmt::format("load user runtime config for {}", appId.empty() ? "global" : appId));

    auto configDir = linglong::common::dir::getUserRuntimeConfigDir();
    if (configDir.empty()) {
        return std::nullopt;
    }

    std::filesystem::path configPath;
    if (appId.empty()) {
        configPath = configDir / "config.json";
    } else {
        configPath = configDir / "apps" / appId / "config.json";
    }

    std::error_code ec;
    if (!std::filesystem::exists(configPath, ec)) {
        return std::nullopt;
    }

    auto result = linglong::utils::loadRuntimeConfig(configPath);
    if (!result) {
        return LINGLONG_ERR(result);
    }

    return std::move(result).value();
}

} // namespace

namespace linglong::utils {

RuntimeConfigure MergeRuntimeConfig(const std::vector<RuntimeConfigure> &configs)
{
    RuntimeConfigure result;

    for (const auto &config : configs) {
        // Merge environment variables
        if (config.env) {
            if (!result.env) {
                result.env = config.env;
            } else {
                for (const auto &[key, value] : *config.env) {
                    result.env->insert_or_assign(key, value);
                }
            }
        }

        // Merge extension definitions
        if (config.extDefs) {
            if (!result.extDefs) {
                result.extDefs = config.extDefs;
            } else {
                for (const auto &[key, value] : *config.extDefs) {
                    auto it = result.extDefs->find(key);
                    if (it == result.extDefs->end()) {
                        result.extDefs->emplace(key, value);
                    } else {
                        it->second.insert(it->second.end(), value.begin(), value.end());
                    }
                }
            }
        }
    }

    return result;
}

linglong::utils::error::Result<RuntimeConfigure>
loadRuntimeConfig(const std::filesystem::path &path)
{
    LINGLONG_TRACE("load runtime config from " + path.string());

    auto result = linglong::utils::serialize::LoadJSONFile<RuntimeConfigure>(path);
    if (!result) {
        return LINGLONG_ERR("failed to load runtime config", result);
    }

    return std::move(result).value();
}

utils::error::Result<std::optional<RuntimeConfigure>> loadRuntimeConfig(const std::string &appId)
{
    LINGLONG_TRACE("load runtime config for app: " + appId);

    // Merge configs in order: user global -> user app
    std::vector<RuntimeConfigure> configs;

    auto userGlobal = loadUserRuntimeConfig("");
    if (!userGlobal) {
        return LINGLONG_ERR(userGlobal);
    }
    if ((*userGlobal).has_value()) {
        configs.emplace_back((*userGlobal).value());
    }

    auto userApp = loadUserRuntimeConfig(appId);
    if (!userApp) {
        return LINGLONG_ERR(userApp);
    }
    if ((*userApp).has_value()) {
        configs.emplace_back((*userApp).value());
    }

    if (configs.empty()) {
        return std::nullopt;
    }

    auto merged = MergeRuntimeConfig(configs);
    return std::move(merged);
}

} // namespace linglong::utils
