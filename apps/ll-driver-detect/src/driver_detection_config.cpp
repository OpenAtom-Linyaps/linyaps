// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "driver_detection_config.h"

#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

namespace linglong::driver::detect {

DriverDetectionConfigManager::DriverDetectionConfigManager(const std::string &configPath)
    : configFilePath(configPath)
{
    // Try to load existing config, if it fails, use defaults
    loadConfig();
}

bool DriverDetectionConfigManager::loadConfig()
{
    std::error_code ec;
    if (!std::filesystem::exists(configFilePath, ec) || ec) {
        // Config file doesn't exist or error checking, use defaults
        return true;
    }

    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json jsonConfig = nlohmann::json::parse(file, nullptr, false);
    file.close();

    if (jsonConfig.is_discarded()) {
        LogW("Failed to parse driver detection config, using defaults.");
        return false;
    }

    // Parse configuration
    if (jsonConfig.contains("neverRemind") && jsonConfig["neverRemind"].is_boolean()) {
        config.neverRemind = jsonConfig["neverRemind"];
    }

    return true;
}

bool DriverDetectionConfigManager::saveConfig()
{
    nlohmann::json jsonConfig;

    jsonConfig["neverRemind"] = config.neverRemind;

    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        return false;
    }

    file << jsonConfig.dump(4); // Pretty print with 4 spaces
    file.close();

    return true;
}

void DriverDetectionConfigManager::recordUserChoice(UserNotificationChoice choice)
{
    choice == UserNotificationChoice::InstallNow ? config.neverRemind = false
                                                 : config.neverRemind = true;

    // Save the updated configuration
    saveConfig();
}

} // namespace linglong::driver::detect
