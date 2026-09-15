// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nvidia_device_detector.h"

#include "linglong/utils/log/log.h"

#include <filesystem>
#include <optional>

namespace linglong::ctk::detect {

NVIDIADeviceDetector::NVIDIADeviceDetector(std::string deviceId,
                                           std::filesystem::path sysModuleRoot)
    : deviceId_(std::move(deviceId))
    , sysModuleRoot_(std::move(sysModuleRoot))
{
}

utils::error::Result<std::optional<std::string>> NVIDIADeviceDetector::detect()
{
    LINGLONG_TRACE("detect kernel module " + deviceId_);

    std::error_code ec;
    const auto modulePath = sysModuleRoot_ / deviceId_;
    if (!std::filesystem::exists(modulePath, ec)) {
        if (ec) {
            return LINGLONG_ERR("failed to check kernel module " + deviceId_ + ": " + ec.message());
        }

        LogD("kernel module {} is not loaded", deviceId_);
        return std::nullopt;
    }

    LogD("kernel module {} is loaded", deviceId_);
    return deviceId_;
}

} // namespace linglong::ctk::detect
