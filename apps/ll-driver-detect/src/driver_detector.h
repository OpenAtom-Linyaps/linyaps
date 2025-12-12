// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <string>

namespace linglong::driver::detect {

struct GraphicsDriverInfo
{
    std::string identify;
    std::string version;
    std::string packageName;
    bool isInstalled = false;
};

class DriverDetector
{
public:
    virtual ~DriverDetector() = default;

    // Main detection method - returns driver info if detected
    virtual utils::error::Result<GraphicsDriverInfo> detect() = 0;

    // Check if the driver package is installed
    virtual utils::error::Result<bool> checkPackageInstalled(const std::string &packageName) = 0;

    // Get the Identify of driver this detector handles
    virtual std::string getDriverIdentify() const = 0;
};

} // namespace linglong::driver::detect
