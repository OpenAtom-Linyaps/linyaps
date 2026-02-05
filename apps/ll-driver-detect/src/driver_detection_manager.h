// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "driver_detector.h"

#include <memory>
#include <vector>

namespace linglong::driver::detect {

// Manager class that coordinates detection of multiple graphics drivers
class DriverDetectionManager
{
public:
    DriverDetectionManager();
    virtual ~DriverDetectionManager() = default;

    // Detect all available graphics drivers on the system
    // Returns all detected drivers, or empty vector if none found
    utils::error::Result<std::vector<GraphicsDriverInfo>> detectAvailableDrivers();
    linglong::utils::error::Result<void> installDriverPackage(const std::vector<GraphicsDriverInfo> &drivers);

protected:
    // Register all available driver detectors
    virtual void registerDetectors();
    std::vector<std::unique_ptr<DriverDetector>> detectors_;
};

} // namespace linglong::driver::detect
