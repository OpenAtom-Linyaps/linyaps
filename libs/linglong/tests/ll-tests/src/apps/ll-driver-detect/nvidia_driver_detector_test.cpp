// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "nvidia_driver_detector.h"

#include <filesystem>
#include <fstream>

using namespace linglong::driver::detect;
using ::testing::_;
using ::testing::Return;

namespace {
std::filesystem::path GetTempVersionFilePath()
{
    return std::filesystem::temp_directory_path() / "nvidia_test_version_file.txt";
}
} // namespace

// A mock class that only mocks checkPackageInstalled, as getDriverVersion is no longer virtual
class TestableNVIDIADriverDetector : public NVIDIADriverDetector
{
public:
    explicit TestableNVIDIADriverDetector(std::string versionFilePath)
        : NVIDIADriverDetector(std::move(versionFilePath))
    {
    }

    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageInstalled,
                (const std::string &),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<void>,
                checkPackageExists,
                (const std::string &),
                (override));
};

class NvidiaDriverDetectorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        versionFilePath = GetTempVersionFilePath();
        std::filesystem::remove(versionFilePath); // Ensure clean slate
    }

    void TearDown() override { std::filesystem::remove(versionFilePath); }

    // Helper to create a mock version file
    void createMockVersionFile(const std::string &content)
    {
        std::ofstream file(versionFilePath);
        ASSERT_TRUE(file.is_open()) << "Failed to create mock version file: " << versionFilePath;
        file << content;
        file.close();
    }

    std::filesystem::path versionFilePath;
};

TEST_F(NvidiaDriverDetectorTest, Detect_Success_PackageNotInstalled)
{
    createMockVersionFile("510.85.02"); // Simplified version format now
    TestableNVIDIADriverDetector detector(versionFilePath.string());
    const std::string expectedVersion = "510-85-02"; // With dots replaced by dashes
    const std::string expectedPackageName =
      std::string(detector.getDriverIdentify()) + "." + expectedVersion;

    EXPECT_CALL(detector, checkPackageExists(expectedPackageName))
      .WillOnce(Return(linglong::utils::error::Result<void>()));
    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(false));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->version, expectedVersion);
    EXPECT_EQ(result->packageName, expectedPackageName);
    EXPECT_FALSE(result->isInstalled);
}

TEST_F(NvidiaDriverDetectorTest, Detect_Success_PackageAlreadyInstalled)
{
    createMockVersionFile("510.85.02");
    TestableNVIDIADriverDetector detector(versionFilePath.string());
    const std::string expectedVersion = "510-85-02";
    const std::string expectedPackageName =
      std::string(detector.getDriverIdentify()) + "." + expectedVersion;

    EXPECT_CALL(detector, checkPackageExists(expectedPackageName))
      .WillOnce(Return(linglong::utils::error::Result<void>()));
    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(true));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->version, expectedVersion);
    EXPECT_EQ(result->packageName, expectedPackageName);
    EXPECT_TRUE(result->isInstalled);
}

TEST_F(NvidiaDriverDetectorTest, Detect_Fails_When_VersionFileDoesNotExist)
{
    std::filesystem::remove(versionFilePath); // Ensure file does not exist
    TestableNVIDIADriverDetector detector(versionFilePath.string());

    EXPECT_CALL(detector, checkPackageInstalled(_)).Times(0); // Should not be called

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message(), "Failed to get NVIDIA driver version");
}

TEST_F(NvidiaDriverDetectorTest, Detect_Fails_When_VersionFileIsEmpty)
{
    createMockVersionFile(""); // Empty file
    TestableNVIDIADriverDetector detector(versionFilePath.string());

    EXPECT_CALL(detector, checkPackageInstalled(_)).Times(0); // Should not be called

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().message(), "Failed to get NVIDIA driver version");
}
