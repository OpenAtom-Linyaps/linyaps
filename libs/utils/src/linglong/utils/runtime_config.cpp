/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
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
loadRuntimeConfigFromDir(const std::filesystem::path &configDir, const std::string &appId)
{
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

    return linglong::utils::loadRuntimeConfig(configPath);
}

linglong::utils::error::Result<std::optional<RuntimeConfigure>>
loadUserRuntimeConfig(const std::string &appId)
{
    LINGLONG_TRACE(
      fmt::format("load user runtime config for {}", appId.empty() ? "global" : appId));

    auto configDir = linglong::common::dir::getUserRuntimeConfigDir();
    if (configDir.empty()) {
        return std::nullopt;
    }

    return loadRuntimeConfigFromDir(configDir, appId);
}

linglong::utils::error::Result<std::optional<RuntimeConfigure>>
loadSystemRuntimeConfig(const std::string &appId)
{
    LINGLONG_TRACE(
      fmt::format("load system runtime config for {}", appId.empty() ? "global" : appId));

    return loadRuntimeConfigFromDir(linglong::common::dir::getSystemRuntimeConfigDir(), appId);
}

} // namespace

namespace linglong::utils {

RuntimeConfigure MergeRuntimeConfig(const std::vector<RuntimeConfigure> &configs)
{
    RuntimeConfigure result;

    for (const auto &config : configs) {
        if (config.disableXdp.has_value()) {
            result.disableXdp = config.disableXdp;
        }

        if (config.deviceMode) {
            if (!result.deviceMode) {
                result.deviceMode = config.deviceMode;
            } else {
                result.deviceMode->insert(result.deviceMode->end(),
                                          config.deviceMode->begin(),
                                          config.deviceMode->end());
            }
        }

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

    return result;
}

utils::error::Result<std::optional<RuntimeConfigure>> loadRuntimeConfig(const std::string &appId)
{
    LINGLONG_TRACE("load runtime config for app: " + appId);

    // Merge configs in order: system global -> system app -> user global -> user app
    std::vector<RuntimeConfigure> configs;

    auto systemGlobal = loadSystemRuntimeConfig("");
    if (!systemGlobal) {
        return LINGLONG_ERR(systemGlobal);
    }
    if (systemGlobal->has_value()) {
        configs.emplace_back(std::move(systemGlobal->value()));
    }

    auto systemApp = loadSystemRuntimeConfig(appId);
    if (!systemApp) {
        return LINGLONG_ERR(systemApp);
    }
    if (systemApp->has_value()) {
        configs.emplace_back(std::move(systemApp->value()));
    }

    auto userGlobal = loadUserRuntimeConfig("");
    if (!userGlobal) {
        return LINGLONG_ERR(userGlobal);
    }
    if (userGlobal->has_value()) {
        configs.emplace_back(std::move(userGlobal->value()));
    }

    auto userApp = loadUserRuntimeConfig(appId);
    if (!userApp) {
        return LINGLONG_ERR(userApp);
    }
    if (userApp->has_value()) {
        configs.emplace_back(std::move(userApp->value()));
    }

    if (configs.empty()) {
        return std::nullopt;
    }

    auto merged = MergeRuntimeConfig(configs);
    return merged;
}

} // namespace linglong::utils
