/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "driver_detection_config.h"

#include <filesystem>

class DriverDetectionConfigTest : public ::testing::Test
{
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
    TempDir temp_dir;
    auto configFilePath = temp_dir.path() / "test_config.json";
    using namespace linglong::driver::detect;

    // Test that construction works
    EXPECT_NO_THROW(DriverDetectionConfigManager manager(configFilePath.string()));
}

TEST_F(DriverDetectionConfigTest, LoadNonExistentConfig)
{
    TempDir temp_dir;
    auto configFilePath = temp_dir.path() / "test_config.json";
    using namespace linglong::driver::detect;

    DriverDetectionConfigManager manager(configFilePath.string());

    // Should handle non-existent file gracefully
    EXPECT_NO_THROW(manager.loadConfig());
}

TEST_F(DriverDetectionConfigTest, SaveAndLoadConfig)
{
    TempDir temp_dir;
    auto configFilePath = temp_dir.path() / "test_config.json";
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
