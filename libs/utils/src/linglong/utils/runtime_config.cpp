/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "runtime_config.h"

#include "linglong/common/dir.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/serialize/json.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

using RuntimeConfigure = linglong::api::types::v1::RuntimeConfigure;

namespace {

bool isRuntimeConfigDropIn(const std::filesystem::path &path)
{
    return path.extension() == ".json";
}

linglong::utils::error::Result<std::optional<RuntimeConfigure>>
loadRuntimeConfigFromDir(const std::filesystem::path &configDir, const std::string &appId)
{
    LINGLONG_TRACE("load runtime config from dir " + configDir.string());

    std::filesystem::path configBaseDir;
    if (appId.empty()) {
        configBaseDir = configDir;
    } else {
        configBaseDir = configDir / "apps" / appId;
    }

    std::vector<RuntimeConfigure> configs;
    auto configPath = configBaseDir / "config.json";
    std::error_code ec;
    if (std::filesystem::exists(configPath, ec)) {
        auto config = linglong::utils::loadRuntimeConfig(configPath);
        if (!config) {
            return LINGLONG_ERR(config);
        }
        configs.emplace_back(std::move(*config));
    }

    auto configDPath = configBaseDir / "config.d";
    if (std::filesystem::is_directory(configDPath, ec)) {
        std::vector<std::filesystem::path> paths;
        for (const auto &entry : std::filesystem::directory_iterator(
               configDPath,
               std::filesystem::directory_options::skip_permission_denied,
               ec)) {
            if (entry.is_regular_file(ec) && isRuntimeConfigDropIn(entry.path())) {
                paths.emplace_back(entry.path());
            }
        }

        std::sort(paths.begin(), paths.end());
        for (const auto &path : paths) {
            auto config = linglong::utils::loadRuntimeConfig(path);
            if (!config) {
                return LINGLONG_ERR(config);
            }
            configs.emplace_back(std::move(*config));
        }
    }

    if (configs.empty()) {
        return std::nullopt;
    }

    return linglong::utils::MergeRuntimeConfig(configs);
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

        if (config.enablePipewire.has_value()) {
            result.enablePipewire = config.enablePipewire;
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

        if (config.devices) {
            if (!result.devices) {
                result.devices = config.devices;
            } else {
                result.devices->insert(result.devices->end(),
                                       config.devices->begin(),
                                       config.devices->end());
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
