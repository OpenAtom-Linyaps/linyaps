/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "driver_detection_config.h"

#include <filesystem>

namespace {
std::filesystem::path GetTempFilePath()
{
    return std::filesystem::temp_directory_path() / ("test_config.json");
}
} // namespace

class DriverDetectionConfigTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        configFilePath = GetTempFilePath();
        // Ensure the file does not exist initially
        std::filesystem::remove(configFilePath);
        ASSERT_FALSE(configFilePath.empty()) << "Failed to create temporary directory";
    }

    void TearDown() override { std::filesystem::remove(configFilePath); }

    std::filesystem::path configFilePath;
};

TEST_F(DriverDetectionConfigTest, DefaultConstruction)
{
    using namespace linglong::driver::detect;

    // Test that default construction works
    DriverDetectionConfig config;
    EXPECT_FALSE(config.neverRemind);
}

TEST_F(DriverDetectionConfigTest, ConfigManagerConstruction)
{
    using namespace linglong::driver::detect;

    // Test that construction works
    EXPECT_NO_THROW(DriverDetectionConfigManager manager(configFilePath.string()));
}

TEST_F(DriverDetectionConfigTest, LoadNonExistentConfig)
{
    using namespace linglong::driver::detect;

    DriverDetectionConfigManager manager(configFilePath.string());

    // Should handle non-existent file gracefully
    EXPECT_NO_THROW(manager.loadConfig());
}

TEST_F(DriverDetectionConfigTest, SaveAndLoadConfig)
{
    using namespace linglong::driver::detect;

    // Create manager and modify config
    DriverDetectionConfigManager manager(configFilePath.string());
    auto config = manager.getConfig();
    config.neverRemind = true;
    manager.setConfig(config);

    // Save config
    EXPECT_TRUE(manager.saveConfig());

    // Create new manager with same path
    DriverDetectionConfigManager manager2(configFilePath.string());
    EXPECT_TRUE(manager2.loadConfig());

    // Verify config was loaded correctly
    auto loadedConfig = manager2.getConfig();
    EXPECT_TRUE(loadedConfig.neverRemind);
}
