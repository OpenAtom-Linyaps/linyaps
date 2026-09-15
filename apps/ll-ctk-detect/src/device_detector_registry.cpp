// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "device_detector_registry.h"

#include "linglong/utils/log/log.h"

#include <utility>

namespace linglong::ctk::detect {

void DeviceDetectorRegistry::add(std::unique_ptr<DeviceDetector> detector)
{
    detectors_.emplace_back(std::move(detector));
}

std::set<std::string> DeviceDetectorRegistry::detectDevices() const
{
    std::set<std::string> detectedDevices;
    for (const auto &detector : detectors_) {
        auto result = detector->detect();
        if (!result) {
            LogW("device detector failed: {}", result.error().message());
            continue;
        }

        if (!*result) {
            continue;
        }

        detectedDevices.emplace(**result);
    }

    return detectedDevices;
}

} // namespace linglong::ctk::detect
