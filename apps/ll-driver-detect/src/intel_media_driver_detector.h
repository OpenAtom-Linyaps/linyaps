// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "driver_detector.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace linglong::driver::detect {

// Detects the Intel video codec (VAAPI) driver package org.deepin.driver.media.intel.
//
// Unlike the NVIDIA package the Intel package carries no version segment in its
// identify: a single package covers both new and legacy Intel GPUs, so the package
// name is the identify itself and the version is resolved from the remote repository.
class IntelMediaDriverDetector : public DriverDetector
{
public:
    static constexpr auto kIntelMediaPackageIdentify = "org.deepin.driver.media.intel";

    // Paths of the kernel modules that expose an Intel GPU. Both the legacy i915
    // driver and the newer xe driver are probed so detection works on old and new
    // Intel hardware. The list is injectable, exactly like the NVIDIA version file
    // path, so tests never depend on the hardware of the machine running them.
    explicit IntelMediaDriverDetector(
      std::vector<std::filesystem::path> intelGpuModulePaths = { "/sys/module/i915",
                                                                 "/sys/module/xe" });
    ~IntelMediaDriverDetector() override = default;

    // Main detection method - returns driver info if detected
    utils::error::Result<GraphicsDriverInfo> detect() override;

    // Check if the driver package is installed
    utils::error::Result<bool> checkPackageInstalled(const std::string &packageName) override;

    // Check if the driver package can be upgraded
    utils::error::Result<bool> checkPackageUpgradable(const std::string &packageName) override;

    // Check if the driver package exists in repo
    virtual utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
    getPackageInfoFromRemoteRepo(const std::string &packageName);

    // Get the type of driver this detector handles
    std::string getDriverIdentify() const override { return kIntelMediaPackageIdentify; }

protected:
    bool compareVersions(std::string_view v1, std::string_view v2) const noexcept;

private:
    // Returns true as soon as one of the configured Intel GPU module paths exists
    bool isIntelGpuPresent() const;
    utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
    getInstalledGraphicsDriverInfo(const std::string &packageName) const;

    std::vector<std::filesystem::path> intelGpuModulePaths_;
};

} // namespace linglong::driver::detect
