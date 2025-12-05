// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nvidia_driver_detector.h"

#include "linglong/utils/command/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <filesystem>
#include <fstream>

namespace linglong::driver::detect {

NVIDIADriverDetector::NVIDIADriverDetector(std::string versionFilePath)
    : versionFilePath_(std::move(versionFilePath))
{
}

utils::error::Result<GraphicsDriverInfo> NVIDIADriverDetector::detect()
{
    LINGLONG_TRACE("NVIDIA driver detection started");

    // Get driver version
    auto version = getDriverVersion();
    if (version.empty()) {
        return LINGLONG_ERR("Failed to get NVIDIA driver version");
    }

    auto linglongPackageName = getDriverIdentify() + "." + version;

    // Check if package exists in repo
    auto existsResult = checkPackageExists(linglongPackageName);
    if (!existsResult) {
        return LINGLONG_ERR("Failed to check if package exists: " + existsResult.error().message());
    }

    // Check if package is installed
    auto installedResult = checkPackageInstalled(linglongPackageName);
    if (!installedResult) {
        return LINGLONG_ERR("Failed to check package installation: "
                            + installedResult.error().message());
    }

    return GraphicsDriverInfo{ getDriverIdentify(),
                               version,
                               linglongPackageName,
                               *installedResult };
}

utils::error::Result<void> NVIDIADriverDetector::checkPackageExists(const std::string &packageName)
{
    LINGLONG_TRACE("Check if NVIDIA driver package exists in repository");
    // Execute ll-cli search command to check driver package existence
    auto ret = linglong::utils::command::Cmd("ll-cli").exec(
      { "search", QString::fromStdString(packageName) });
    if (!ret) {
        return LINGLONG_ERR("Search command failed: " + ret.error().message());
    }

    if (!ret->contains(QString::fromStdString(packageName))) {
        return LINGLONG_ERR("Driver package not found in linglong repo: " + packageName);
    }
    return LINGLONG_OK;
}

utils::error::Result<bool>
NVIDIADriverDetector::checkPackageInstalled(const std::string &packageName)
{
    LINGLONG_TRACE("Check if NVIDIA driver package is installed");

    try {
        // First execute ll-cli info to get the package info
        auto listResult =
          linglong::utils::command::Cmd("ll-cli").exec({ "info", packageName.c_str() });

        if (!listResult) {
            LogD("Can not get package info with `ll-cli info`, maybe the package is not installed: {}",
                 listResult.error().message());
            return false;
        }

        return true;
    } catch (const std::exception &e) {
        return LINGLONG_ERR("Failed to check package installation: " + std::string(e.what()));
    }
}

std::string NVIDIADriverDetector::getDriverVersion()
{
    std::string version;

    if (!std::filesystem::exists(versionFilePath_)) {
        LogW("NVIDIA version file not found: {}", versionFilePath_);
        return version;
    }

    std::ifstream versionFile(versionFilePath_);
    if (versionFile) {
        versionFile >> version;

        std::replace(version.begin(), version.end(), '.', '-');
    }

    return version;
}

} // namespace linglong::driver::detect
