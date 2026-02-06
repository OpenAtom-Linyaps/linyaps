// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "driver_detection_manager.h"

#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"
#include "nvidia_driver_detector.h"

namespace linglong::driver::detect {

DriverDetectionManager::DriverDetectionManager()
{
    registerDetectors();
}

utils::error::Result<std::vector<GraphicsDriverInfo>>
DriverDetectionManager::detectAvailableDrivers()
{
    std::vector<GraphicsDriverInfo> detectedDrivers;

    for (const auto &detector : detectors_) {
        auto detectionResult = detector->detect();
        if (!detectionResult) {
            LogE("Driver detection failed {}: {}",
                 detector->getDriverIdentify(),
                 detectionResult.error().message());
            continue;
        }

        detectedDrivers.emplace_back(*detectionResult);
        LogD("Detected driver: {} version: {}",
             detectionResult->packageName,
             detectionResult->packageVersion);
    }

    return detectedDrivers;
}

void DriverDetectionManager::registerDetectors()
{
    // Register NVIDIA detector
    detectors_.push_back(std::make_unique<NVIDIADriverDetector>());

    // TODO: Add other driver detector

    LogD("Registered {} driver detectors", detectors_.size());
}

linglong::utils::error::Result<void>
DriverDetectionManager::installDriverPackage(const std::vector<GraphicsDriverInfo> &drivers)
{
    LINGLONG_TRACE("installDriverPackage")

    try {
        for (const auto &info : drivers) {
            LogD("Processing driver: Identify={}, Version={}, Package={}",
                 info.identify,
                 info.packageVersion,
                 info.packageName);

            // Execute ll-cli install command to install the package
            auto packageRef = info.packageName + "/" + info.packageVersion;
            auto ret = linglong::utils::Cmd("ll-cli").exec(
              { "install", packageRef, "--repo", info.repoName });
            if (!ret) {
                return LINGLONG_ERR("Installation command failed: " + ret.error().message());
            }

            LogD("Driver package installation command executed successfully: {}", *ret);
        }

        return LINGLONG_OK;
    } catch (const std::exception &e) {
        return LINGLONG_ERR("Failed to install driver package: " + std::string(e.what()));
    }
}

} // namespace linglong::driver::detect
