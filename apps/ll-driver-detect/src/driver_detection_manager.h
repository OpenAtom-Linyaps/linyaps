// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "driver_detector.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace linglong::driver::detect {

// Result of driver detection for all available drivers
struct DriverDetectionResult
{
    std::vector<GraphicsDriverInfo> detectedDrivers;

    bool hasAvailableDrivers() const
    {
        return std::any_of(detectedDrivers.begin(), detectedDrivers.end(), [](const auto &driver) {
            return !driver.isInstalled;
        });
    }
};

// Manager class that coordinates detection of multiple graphics drivers
class DriverDetectionManager
{
public:
    DriverDetectionManager();
    virtual ~DriverDetectionManager() = default;

    // Detect all available graphics drivers on the system
    // Returns all detected drivers, or empty vector if none found
    utils::error::Result<DriverDetectionResult> detectAllDrivers();

protected:
    // Register all available driver detectors
    virtual void registerDetectors();

    std::vector<std::unique_ptr<DriverDetector>> detectors_;
};

} // namespace linglong::driver::detect
