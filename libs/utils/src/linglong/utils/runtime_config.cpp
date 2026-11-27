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
#include <iterator>

using RuntimeConfigure = linglong::api::types::v1::RuntimeConfigure;
using linglong::api::types::v1::CdiDeviceRequest;
using linglong::api::types::v1::Device;

namespace {

bool isRuntimeConfigDropIn(const std::filesystem::path &path)
{
    return path.extension() == ".json";
}

using linglong::utils::error::Result;

Result<std::vector<std::filesystem::path>> dropInPaths(const std::filesystem::path &directory)
{
    LINGLONG_TRACE("list runtime config drop-ins in " + directory.string());
    std::error_code ec;
    std::filesystem::directory_iterator entry(
      directory,
      std::filesystem::directory_options::skip_permission_denied,
      ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
        return LINGLONG_OK;
    }
    std::vector<std::filesystem::path> paths;
    for (const std::filesystem::directory_iterator end; !ec && entry != end; entry.increment(ec)) {
        std::error_code statusError;
        if (entry->is_regular_file(statusError) && isRuntimeConfigDropIn(entry->path())) {
            paths.emplace_back(entry->path());
        }
    }
    if (ec) {
        return LINGLONG_ERR("scan runtime config drop-ins", ec);
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

Result<std::vector<RuntimeConfigure>> readDropIns(const std::filesystem::path &directory)
{
    LINGLONG_TRACE("load runtime config drop-ins from " + directory.string());
    auto paths = dropInPaths(directory);
    if (!paths) {
        return LINGLONG_ERR(paths);
    }
    std::vector<RuntimeConfigure> configs;
    for (const auto &path : *paths) {
        auto config = linglong::utils::loadRuntimeConfig(path);
        if (!config) {
            return LINGLONG_ERR(config);
        }
        configs.emplace_back(std::move(*config));
    }
    return configs;
}

void resolveCdiSpecPaths(std::vector<Device> &devices, const std::filesystem::path &directory)
{
    for (auto &device : devices) {
        auto *request = std::get_if<CdiDeviceRequest>(&device);
        if (request && !request->spec.empty()
            && std::filesystem::path(request->spec).is_relative()) {
            request->spec = (directory / request->spec).string();
        }
    }
}

void resolveCdiSpecPaths(RuntimeConfigure &config, const std::filesystem::path &directory)
{
    if (config.devices) {
        resolveCdiSpecPaths(*config.devices, directory);
    }

    if (config.instances) {
        for (auto &[name, instance] : *config.instances) {
            resolveCdiSpecPaths(instance, directory);
        }
    }
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

    auto dropIns = readDropIns(configBaseDir / "config.d");
    if (!dropIns) {
        return LINGLONG_ERR(dropIns);
    }
    configs.insert(configs.end(),
                   std::make_move_iterator(dropIns->begin()),
                   std::make_move_iterator(dropIns->end()));

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

        if (config.enableAtspi.has_value()) {
            result.enableAtspi = config.enableAtspi;
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

    std::error_code ec;
    const auto directory = std::filesystem::absolute(path, ec).parent_path();
    if (ec) {
        return LINGLONG_ERR("locate runtime config directory", ec);
    }
    resolveCdiSpecPaths(*result, directory);
    return result;
}

utils::error::Result<std::vector<Device>>
loadCdiDeviceDropIns(const std::filesystem::path &directory)
{
    LINGLONG_TRACE("load generated CDI devices");
    auto paths = dropInPaths(directory);
    if (!paths) {
        return LINGLONG_ERR(paths);
    }
    if (paths->empty()) {
        return LINGLONG_OK;
    }

    std::error_code ec;
    const auto configDir = std::filesystem::absolute(directory, ec);
    if (ec) {
        return LINGLONG_ERR("locate CDI config directory", ec);
    }
    std::vector<Device> devices;
    for (const auto &path : *paths) {
        auto config = serialize::LoadJSONFile<nlohmann::json>(path);
        if (!config) {
            return LINGLONG_ERR("load CDI config " + path.string(), config);
        }
        const auto list = config->find("devices");
        if (list == config->end() || list->is_null()) {
            continue;
        }
        auto parsed = serialize::LoadJSON<std::vector<Device>>(*list);
        if (!parsed) {
            return LINGLONG_ERR("load CDI devices in " + path.string(), parsed);
        }
        resolveCdiSpecPaths(*parsed, configDir);
        devices.insert(devices.end(),
                       std::make_move_iterator(parsed->begin()),
                       std::make_move_iterator(parsed->end()));
    }
    return devices;
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
    merged.instances.reset();
    return merged;
}

} // namespace linglong::utils
