// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "container_tool_config.h"

#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>

namespace linglong::ctk::detect {

namespace {

std::optional<std::vector<ContainerToolSpec>> parseTools(const nlohmann::json &json)
{
    auto iter = json.find("tools");
    if (iter == json.end() || !iter->is_array()) {
        return std::nullopt;
    }

    std::vector<ContainerToolSpec> tools;
    for (const auto &entry : *iter) {
        if (!entry.is_object()) {
            continue;
        }

        ContainerToolSpec spec;
        auto deviceId = entry.find("deviceId");
        auto packages = entry.find("packages");
        if (deviceId == entry.end() || !deviceId->is_string()
            || deviceId->get<std::string>().empty() || packages == entry.end()
            || !packages->is_array()) {
            continue;
        }

        spec.deviceId = deviceId->get<std::string>();
        for (const auto &package : *packages) {
            if (!package.is_string()) {
                continue;
            }

            auto packageName = package.get<std::string>();
            if (packageName.empty()) {
                continue;
            }

            if (std::find(spec.packages.begin(), spec.packages.end(), packageName)
                != spec.packages.end()) {
                continue;
            }
            spec.packages.emplace_back(std::move(packageName));
        }

        if (spec.packages.empty()) {
            continue;
        }

        tools.emplace_back(std::move(spec));
    }

    return tools;
}

std::optional<std::map<std::string, bool>> parseNeverRemind(const nlohmann::json &json)
{
    auto iter = json.find("neverRemind");
    if (iter == json.end() || !iter->is_object()) {
        return std::nullopt;
    }

    std::map<std::string, bool> neverRemind;
    for (auto it = iter->begin(); it != iter->end(); ++it) {
        if (it.value().is_boolean()) {
            neverRemind[it.key()] = it.value().get<bool>();
        }
    }
    return neverRemind;
}

utils::error::Result<nlohmann::json> loadJsonFile(const std::filesystem::path &path)
{
    LINGLONG_TRACE("load config file: " + path.string())

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return nlohmann::json::object();
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return LINGLONG_ERR("failed to open config file: " + path.string());
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception &e) {
        return LINGLONG_ERR("failed to parse config file " + path.string() + ": " + e.what());
    }
    return json;
}

} // namespace

ContainerToolConfigManager::ContainerToolConfigManager(std::filesystem::path systemConfigPath,
                                                       std::filesystem::path userConfigPath)
    : systemConfigPath_(std::move(systemConfigPath))
    , userConfigPath_(std::move(userConfigPath))
{
}

utils::error::Result<void> ContainerToolConfigManager::load()
{
    LINGLONG_TRACE("load ll-ctk-detect config")

    auto systemJson = loadJsonFile(systemConfigPath_);
    if (!systemJson) {
        return LINGLONG_ERR(systemJson.error());
    }

    if (auto tools = parseTools(*systemJson)) {
        tools_ = std::move(*tools);
    }

    auto userJson = loadJsonFile(userConfigPath_);
    if (!userJson) {
        return LINGLONG_ERR(userJson.error());
    }

    if (auto tools = parseTools(*userJson)) {
        mergeTools(*tools);
    }
    if (auto neverRemind = parseNeverRemind(*userJson)) {
        neverRemind_ = std::move(*neverRemind);
    }

    LogD("loaded {} container tool(s)", tools_.size());
    return LINGLONG_OK;
}

bool ContainerToolConfigManager::isNeverRemind(const std::string &deviceId) const
{
    auto iter = neverRemind_.find(deviceId);
    return iter != neverRemind_.end() && iter->second;
}

void ContainerToolConfigManager::setNeverRemind(const std::string &deviceId, bool neverRemind)
{
    neverRemind_[deviceId] = neverRemind;
}

utils::error::Result<void> ContainerToolConfigManager::saveUserConfig() const
{
    LINGLONG_TRACE("save ll-ctk-detect user config")

    auto json = loadJsonFile(userConfigPath_);
    if (!json) {
        return LINGLONG_ERR(json.error());
    }

    nlohmann::json neverRemind = nlohmann::json::object();
    for (const auto &[id, remind] : neverRemind_) {
        neverRemind[id] = remind;
    }
    (*json)["neverRemind"] = std::move(neverRemind);

    std::error_code ec;
    std::filesystem::create_directories(userConfigPath_.parent_path(), ec);
    if (ec) {
        return LINGLONG_ERR("failed to create user config directory: " + ec.message());
    }

    std::ofstream file(userConfigPath_);
    if (!file.is_open()) {
        return LINGLONG_ERR("failed to open user config file for writing: "
                            + userConfigPath_.string());
    }

    file << json->dump(4) << '\n';
    if (!file.good()) {
        return LINGLONG_ERR("failed to write user config file: " + userConfigPath_.string());
    }
    return LINGLONG_OK;
}

void ContainerToolConfigManager::mergeTools(const std::vector<ContainerToolSpec> &tools)
{
    for (const auto &tool : tools) {
        auto iter =
          std::find_if(tools_.begin(), tools_.end(), [&tool](const ContainerToolSpec &existing) {
              return existing.deviceId == tool.deviceId;
          });
        if (iter == tools_.end()) {
            tools_.emplace_back(tool);
        } else {
            *iter = tool;
        }
    }
}

} // namespace linglong::ctk::detect
