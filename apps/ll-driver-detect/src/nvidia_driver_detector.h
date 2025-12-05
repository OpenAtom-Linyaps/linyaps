// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "driver_detector.h"

#include <string>

namespace linglong::driver::detect {

class NVIDIADriverDetector : public DriverDetector
{
public:
    static constexpr auto kNvidiaPackageIdentify = "org.deepin.driver.display.nvidia";
    static constexpr auto kDefaultNvidiaVersionFile = "/sys/module/nvidia/version";

    explicit NVIDIADriverDetector(std::string versionFilePath = kDefaultNvidiaVersionFile);
    ~NVIDIADriverDetector() override = default;

    // Main detection method - returns driver info if detected
    utils::error::Result<GraphicsDriverInfo> detect() override;

    // Check if the driver package is installed
    utils::error::Result<bool> checkPackageInstalled(const std::string &packageName) override;

    // Check if the driver package exists in repo
    virtual utils::error::Result<void> checkPackageExists(const std::string &packageName);

    // Get the type of driver this detector handles
    std::string getDriverIdentify() const override { return kNvidiaPackageIdentify; }

private:
    // Get driver version from the specified file path
    std::string getDriverVersion();

    std::string versionFilePath_;
};

} // namespace linglong::driver::detect
