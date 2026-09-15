// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "device_detector.h"

#include <filesystem>
#include <string>

namespace linglong::ctk::detect {

class NVIDIADeviceDetector : public DeviceDetector
{
public:
    explicit NVIDIADeviceDetector(std::string deviceId,
                                  std::filesystem::path sysModuleRoot = "/sys/module");

    utils::error::Result<std::optional<std::string>> detect() override;

private:
    std::string deviceId_;
    std::filesystem::path sysModuleRoot_;
};

} // namespace linglong::ctk::detect
