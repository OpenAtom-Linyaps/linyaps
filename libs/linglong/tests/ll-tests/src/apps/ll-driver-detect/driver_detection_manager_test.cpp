// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "driver_detection_manager.h"
#include "driver_detector.h"

#include <memory>
#include <vector>

using namespace linglong::driver::detect;

// A custom detector that returns a specific driver info for successful detection
class FakeSuccessDetector : public DriverDetector
{
public:
    FakeSuccessDetector(std::string identify, std::string version, bool installed)
        : info_{ std::move(identify), std::move(version), "some-package", installed }
    {
    }

    linglong::utils::error::Result<GraphicsDriverInfo> detect() override { return info_; }

    linglong::utils::error::Result<bool> checkPackageInstalled(const std::string &) override
    {
        return info_.isInstalled;
    }

    std::string getDriverIdentify() const override { return info_.identify; }

private:
    GraphicsDriverInfo info_;
};

// A custom detector that always fails
class FakeFailureDetector : public DriverDetector
{
public:
    linglong::utils::error::Result<GraphicsDriverInfo> detect() override
    {
        LINGLONG_TRACE("Fake detector failed as intended")
        return LINGLONG_ERR("Fake detector failed as intended");
    }

    linglong::utils::error::Result<bool> checkPackageInstalled(const std::string &) override
    {
        LINGLONG_TRACE("Fake detector failed as intended")
        return LINGLONG_ERR("Should not be called");
    }

    std::string getDriverIdentify() const override { return "fake-failure"; }
};

// A test-specific subclass of DriverDetectionManager to allow for mock injection
class TestableDriverDetectionManager : public DriverDetectionManager
{
public:
    // Override to prevent registration of real detectors
    void registerDetectors() override { }

    // Expose a way to add mock/fake detectors for tests
    void addDetector(std::unique_ptr<DriverDetector> detector)
    {
        detectors_.push_back(std::move(detector));
    }
};

class DriverDetectionManagerTest : public ::testing::Test
{
};

TEST_F(DriverDetectionManagerTest, DetectAllDrivers_NoDetectors)
{
    TestableDriverDetectionManager manager;
    auto result = manager.detectAllDrivers();
    ASSERT_TRUE(result.has_value());
}
