// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <string>

namespace linglong::driver::detect {

enum class UserNotificationChoice { InstallNow, NeverRemind };

struct DriverDetectionConfig
{
    bool neverRemind = false;
};

class DriverDetectionConfigManager
{
public:
    explicit DriverDetectionConfigManager(const std::string &configPath);
    ~DriverDetectionConfigManager() = default;

    // Load configuration from file
    bool loadConfig();

    // Save configuration to file
    bool saveConfig();

    // Get current configuration
    DriverDetectionConfig getConfig() const { return config; }

    // Update configuration
    void setConfig(const DriverDetectionConfig &newConfig) { config = newConfig; }

    // Check if we should show notification (simplified - no version-specific logic)
    bool shouldShowNotification() const { return !config.neverRemind; }

    // Record user choice (simplified - no version parameter needed)
    void recordUserChoice(UserNotificationChoice choice);

private:
    std::string configFilePath;
    DriverDetectionConfig config;
};

} // namespace linglong::driver::detect
