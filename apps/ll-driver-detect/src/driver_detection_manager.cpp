// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "driver_detection_manager.h"

#include "linglong/utils/log/log.h"
#include "nvidia_driver_detector.h"

namespace linglong::driver::detect {

DriverDetectionManager::DriverDetectionManager()
{
    registerDetectors();
}

utils::error::Result<DriverDetectionResult> DriverDetectionManager::detectAllDrivers()
{
    DriverDetectionResult result;

    for (const auto &detector : detectors_) {
        auto detectionResult = detector->detect();
        if (!detectionResult) {
            LogE("Driver detection failed {}: {}",
                 detector->getDriverIdentify(),
                 detectionResult.error().message());
            continue;
        }

        result.detectedDrivers.emplace_back(*detectionResult);
        LogD("Detected driver: {} version: {}",
             detectionResult->packageName,
             detectionResult->version);
    }

    return result;
}

void DriverDetectionManager::registerDetectors()
{
    // Register NVIDIA detector
    detectors_.push_back(std::make_unique<NVIDIADriverDetector>());

    // TODO: Add other driver detector

    LogD("Registered {} driver detectors", detectors_.size());
}

} // namespace linglong::driver::detect
