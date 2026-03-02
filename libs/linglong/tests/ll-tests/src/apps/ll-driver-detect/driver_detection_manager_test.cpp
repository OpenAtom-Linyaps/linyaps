// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "driver_detection_manager.h"
#include "driver_detector.h"

#include <memory>
#include <vector>

using namespace linglong::driver::detect;
using namespace testing;

// Mock class for DriverDetector
class MockDriverDetector : public DriverDetector
{
public:
    MOCK_METHOD(linglong::utils::error::Result<GraphicsDriverInfo>, detect, (), (override));
    MOCK_METHOD(std::string, getDriverIdentify, (), (const, noexcept, override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageInstalled,
                (const std::string &packageName),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageUpgradable,
                (const std::string &packageName),
                (override));
};

class DriverDetectionManagerTest : public ::testing::Test
{
};

TEST_F(DriverDetectionManagerTest, DetectAvailableDriversSuccess)
{
    auto mockDetector = std::make_unique<MockDriverDetector>();

    EXPECT_CALL(*mockDetector, detect())
      .WillOnce(Return(linglong::utils::error::Result<GraphicsDriverInfo>(
        GraphicsDriverInfo{ "nvidia", "nvidia-driver", "1.0", "stable" })));

    std::vector<std::unique_ptr<DriverDetector>> detectors;
    detectors.push_back(std::move(mockDetector));

    DriverDetectionManager manager(std::move(detectors));
    auto result = manager.detectAvailableDrivers();

    ASSERT_TRUE(result);
    ASSERT_EQ(result->size(), 1);
    EXPECT_EQ(result->at(0).identify, "nvidia");
    EXPECT_EQ(result->at(0).packageName, "nvidia-driver");
    EXPECT_EQ(result->at(0).packageVersion, "1.0");
    EXPECT_EQ(result->at(0).repoName, "stable");
}

TEST_F(DriverDetectionManagerTest, DetectAvailableDriversFailure)
{
    LINGLONG_TRACE("Testing detection failure scenario");
    auto mockDetector = std::make_unique<MockDriverDetector>();

    EXPECT_CALL(*mockDetector, detect()).WillOnce(Return(LINGLONG_ERR("Detection error")));

    EXPECT_CALL(*mockDetector, getDriverIdentify()).WillOnce(Return("nvidia"));

    std::vector<std::unique_ptr<DriverDetector>> detectors;
    detectors.push_back(std::move(mockDetector));

    DriverDetectionManager manager(std::move(detectors));
    auto result = manager.detectAvailableDrivers();

    ASSERT_TRUE(result);
    EXPECT_TRUE(result->empty());
}
