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

        if (config.env) {
            if (!result.env) {
                result.env = config.env;
            } else {
                for (const auto &[key, value] : *config.env) {
                    result.env->insert_or_assign(key, value);
                }
            }
        }

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

        if (config.mounts) {
            if (!result.mounts) {
                result.mounts = config.mounts;
            } else {
                result.mounts->insert(result.mounts->end(),
                                      config.mounts->begin(),
                                      config.mounts->end());
            }
        }

        if (config.instances) {
            if (!result.instances) {
                result.instances = config.instances;
            } else {
                for (const auto &[instanceName, instanceCfg] : *config.instances) {
                    auto it = result.instances->find(instanceName);
                    if (it == result.instances->end()) {
                        result.instances->emplace(instanceName, instanceCfg);
                    } else {
                        std::vector<RuntimeConfigure> instanceConfigs;
                        instanceConfigs.emplace_back(std::move(it->second));
                        instanceConfigs.emplace_back(instanceCfg);
                        it->second = MergeRuntimeConfig(instanceConfigs);
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

utils::error::Result<std::optional<RuntimeConfigure>> loadRuntimeConfig(const std::string &appId,
                                                                        const std::string &instance)
{
    LINGLONG_TRACE("load runtime config for app: " + appId
                   + ", instance: " + (instance.empty() ? "(default)" : instance));

    std::vector<std::filesystem::path> configDirs;
    auto systemConfigDir = linglong::common::dir::getSystemRuntimeConfigDir();
    if (!systemConfigDir.empty()) {
        configDirs.emplace_back(std::move(systemConfigDir));
    }
    auto userConfigDir = linglong::common::dir::getUserRuntimeConfigDir();
    if (!userConfigDir.empty()) {
        configDirs.emplace_back(std::move(userConfigDir));
    }
    return loadRuntimeConfig(configDirs, appId, instance);
}

utils::error::Result<std::optional<RuntimeConfigure>>
loadRuntimeConfig(const std::vector<std::filesystem::path> &configDirs,
                  const std::string &appId,
                  const std::string &instance)
{
    LINGLONG_TRACE("load runtime config for app: " + appId
                   + ", instance: " + (instance.empty() ? "(default)" : instance));

    std::vector<RuntimeConfigure> configs;

    for (const auto &configDir : configDirs) {
        if (configDir.empty()) {
            continue;
        }

        auto global = loadRuntimeConfigFromDir(configDir, "");
        if (!global) {
            return LINGLONG_ERR(global);
        }
        if (global->has_value()) {
            configs.emplace_back(std::move(global->value()));
        }

        if (!appId.empty()) {
            auto app = loadRuntimeConfigFromDir(configDir, appId);
            if (!app) {
                return LINGLONG_ERR(app);
            }
            if (app->has_value()) {
                configs.emplace_back(std::move(app->value()));
            }
        }
    }

    if (configs.empty()) {
        return std::nullopt;
    }

    auto merged = MergeRuntimeConfig(configs);

    if (!instance.empty() && merged.instances
        && merged.instances->find(instance) != merged.instances->end()) {
        auto instanceConfig = std::move((*merged.instances)[instance]);
        std::vector<RuntimeConfigure> toMerge;
        toMerge.emplace_back(std::move(merged));
        toMerge.emplace_back(std::move(instanceConfig));
        merged = MergeRuntimeConfig(toMerge);
    }

    merged.instances = std::nullopt;

    return merged;
}

} // namespace linglong::utils
