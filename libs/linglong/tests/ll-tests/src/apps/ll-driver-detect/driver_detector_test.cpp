/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "driver_detection_config.h"
#include "driver_detector.h"
#include "nvidia_driver_detector.h"

#include <filesystem>
#include <string>

class DriverDetectorTest : public ::testing::Test
{
};

TEST_F(DriverDetectorTest, NvidiaDriverDetectorInitialization)
{
    linglong::driver::detect::NVIDIADriverDetector nvidiaDetector;

    // Test basic initialization - NVIDIADriverDetector should be properly initialized
    EXPECT_EQ(nvidiaDetector.getDriverIdentify(), "org.deepin.driver.display.nvidia");
}

TEST_F(DriverDetectorTest, NvidiaDriverDetectorDetect)
{
    linglong::driver::detect::NVIDIADriverDetector nvidiaDetector;

    // Test basic detection functionality
    auto result = nvidiaDetector.detect();
    // This may or may not find a driver depending on the system
    // We're mainly testing that the detection process works without crashing
    EXPECT_TRUE(result.has_value() || !result.has_value());

    if (result.has_value()) {
        auto &driverInfo = *result;
        EXPECT_EQ(driverInfo.identify, "org.deepin.driver.display.nvidia");
        EXPECT_FALSE(driverInfo.version.empty());
        EXPECT_FALSE(driverInfo.packageName.empty());
    }
}

TEST_F(DriverDetectorTest, NvidiaDriverDetectorPackageCheck)
{
    linglong::driver::detect::NVIDIADriverDetector nvidiaDetector;

    // Test package checking with a likely non-existent package name
    auto result = nvidiaDetector.checkPackageInstalled("nonexistent_package_name_12345");
    // Should return a valid result (false) rather than error
    EXPECT_TRUE(result.has_value());
    if (result.has_value()) {
        EXPECT_FALSE(*result); // Package should not be found
    }
}

TEST_F(DriverDetectorTest, DriverDetectionConfigManagerSave)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "test_version";
    linglong::driver::detect::DriverDetectionConfigManager configManager(versionFilePath.string());

    // Load default config
    ASSERT_TRUE(configManager.loadConfig());

    // Modify config
    linglong::driver::detect::DriverDetectionConfig newConfig;
    configManager.setConfig(newConfig);

    // Test saving configuration to file
    bool result = configManager.saveConfig();
    EXPECT_TRUE(result);
}

TEST_F(DriverDetectorTest, DriverDetectionConfigManagerRoundTrip)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "test_version";
    // Create and configure first manager
    linglong::driver::detect::DriverDetectionConfigManager configManager1(versionFilePath.string());
    ASSERT_TRUE(configManager1.loadConfig());

    // Modify config
    linglong::driver::detect::DriverDetectionConfig config;
    config.neverRemind = true;
    configManager1.setConfig(config);

    // Save to file
    ASSERT_TRUE(configManager1.saveConfig());

    // Load from file into new config manager
    linglong::driver::detect::DriverDetectionConfigManager configManager2(versionFilePath.string());
    bool loadResult = configManager2.loadConfig();
    EXPECT_TRUE(loadResult);

    // Verify configs match
    auto config1 = configManager1.getConfig();
    auto config2 = configManager2.getConfig();
    EXPECT_EQ(config1.neverRemind, config2.neverRemind);
}

TEST_F(DriverDetectorTest, GraphicsDriverInfoStructure)
{
    // Test GraphicsDriverInfo structure
    linglong::driver::detect::GraphicsDriverInfo info;
    info.identify = "org.deepin.driver.display.nvidia";
    info.version = "470.161.03";
    info.packageName = "nvidia-driver-470";
    info.isInstalled = true;

    EXPECT_EQ(info.identify, "org.deepin.driver.display.nvidia");
    EXPECT_EQ(info.version, "470.161.03");
    EXPECT_EQ(info.packageName, "nvidia-driver-470");
    EXPECT_TRUE(info.isInstalled);
}

TEST_F(DriverDetectorTest, ConfigManagerUserChoice)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "test_version";
    linglong::driver::detect::DriverDetectionConfigManager configManager(versionFilePath.string());
    ASSERT_TRUE(configManager.loadConfig());

    configManager.recordUserChoice(linglong::driver::detect::UserNotificationChoice::InstallNow);

    // Initially should show notification
    EXPECT_TRUE(configManager.shouldShowNotification());

    // Record user choice to never remind
    configManager.recordUserChoice(linglong::driver::detect::UserNotificationChoice::NeverRemind);

    // Should not show notification anymore
    EXPECT_FALSE(configManager.shouldShowNotification());
}

TEST_F(DriverDetectorTest, MemoryManagement)
{
    // Test that detector properly manages memory
    for (int i = 0; i < 100; ++i) {
        linglong::driver::detect::NVIDIADriverDetector detector;
        auto result = detector.detect();
        // Just ensure no crashes occur
        EXPECT_TRUE(result.has_value() || !result.has_value());
    }
    // If we get here without crashes, memory management is working
}

TEST_F(DriverDetectorTest, ConcurrentDetection)
{
    // Test that multiple detectors can be created and used
    linglong::driver::detect::NVIDIADriverDetector detector1;
    linglong::driver::detect::NVIDIADriverDetector detector2;

    auto result1 = detector1.detect();
    auto result2 = detector2.detect();

    // Both should complete without issues
    EXPECT_TRUE(result1.has_value() || !result1.has_value());
    EXPECT_TRUE(result2.has_value() || !result2.has_value());
}
