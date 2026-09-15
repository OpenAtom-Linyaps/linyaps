// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "device_detector.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace linglong::ctk::detect {

class DeviceDetectorRegistry
{
public:
    void add(std::unique_ptr<DeviceDetector> detector);

    std::set<std::string> detectDevices() const;

private:
    std::vector<std::unique_ptr<DeviceDetector>> detectors_;
};

} // namespace linglong::ctk::detect
