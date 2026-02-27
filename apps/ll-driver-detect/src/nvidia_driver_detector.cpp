// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nvidia_driver_detector.h"

#include "linglong/package/version.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
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
    auto packageInfoResult = getPackageInfoFromRemoteRepo(linglongPackageName);
    if (!packageInfoResult) {
        return LINGLONG_ERR("Failed to get package info from remote repo: "
                            + packageInfoResult.error().message());
    }

    auto installedResult = checkPackageInstalled(linglongPackageName);
    if (!installedResult) {
        return LINGLONG_ERR("Failed to check if package is installed: "
                            + installedResult.error().message());
    }

    if (*installedResult) {
        if (!packageInfoResult->first) {
            LogW("NVIDIA driver package is installed locally but not found in remote repo: {}",
                 linglongPackageName);
            return LINGLONG_ERR("Cannot find NVIDIA driver package in remote repo.");
        }

        auto upgradableResult = checkPackageUpgradable(linglongPackageName);
        if (!upgradableResult) {
            return LINGLONG_ERR("Failed to check if package is upgradable: "
                                + upgradableResult.error().message());
        }
        if (*upgradableResult) {
            LogD("NVIDIA driver package can be upgraded : {}, install from remote repo.",
                 linglongPackageName);
            return packageInfoResult->second;
        }
        return LINGLONG_ERR("NVIDIA driver package is already installed and up-to-date.");
    }

    if (!packageInfoResult->first) {
        return LINGLONG_ERR("NVIDIA driver package not found in remote repo");
    }

    LogD("NVIDIA driver package is not installed, install from remote repo.");
    return packageInfoResult->second;
}

utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
NVIDIADriverDetector::getPackageInfoFromRemoteRepo(const std::string &packageName)
{
    using json = nlohmann::json;

    LINGLONG_TRACE("Check if NVIDIA driver package exists in repository");

    GraphicsDriverInfo driverInfo{ getDriverIdentify(), packageName };
    // Execute ll-cli search command to check driver package existence
    auto ret = linglong::utils::Cmd("ll-cli").exec({ "--json", "search", packageName });
    if (!ret) {
        return LINGLONG_ERR("Search command failed: " + ret.error().message());
    }

    bool found = false;

    try {
        json j = json::parse(*ret);

        for (const auto &[category, apps] : j.items()) {
            if (!apps.is_array())
                continue;

            for (const auto &item : apps) {
                auto it = item.find("version");
                if (it == item.end() || !it->is_string())
                    continue;

                std::string_view currentVersionStr = it->get<std::string_view>();

                if (!found || compareVersions(currentVersionStr, driverInfo.packageVersion)) {
                    driverInfo.repoName = category;
                    driverInfo.packageVersion = currentVersionStr;
                    found = true;
                }
            }
        }
    } catch (const nlohmann::json::parse_error &e) {
        return LINGLONG_ERR("Failed to parse search result JSON: " + std::string(e.what()));
    }

    return std::make_pair(found, driverInfo);
}

utils::error::Result<bool>
NVIDIADriverDetector::checkPackageInstalled(const std::string &packageName)
{
    LINGLONG_TRACE("Check if NVIDIA driver package is installed");

    auto installedInfo = getInstalledGraphicsDriverInfo(packageName);
    if (!installedInfo) {
        return LINGLONG_ERR(installedInfo);
    }

    return installedInfo->first;
}

utils::error::Result<bool>
NVIDIADriverDetector::checkPackageUpgradable(const std::string &packageName)
{
    LINGLONG_TRACE("Check if NVIDIA driver package is upgradable");

    auto upgradableDriverInfo = getPackageInfoFromRemoteRepo(packageName);
    if (!upgradableDriverInfo) {
        return LINGLONG_ERR("Failed to get upgradable package info: "
                            + upgradableDriverInfo.error().message());
    }

    if (!upgradableDriverInfo->first) {
        return LINGLONG_ERR("NVIDIA driver package not found in remote repo");
    }

    auto installedDriverInfo = getInstalledGraphicsDriverInfo(packageName);
    if (!installedDriverInfo) {
        return LINGLONG_ERR("Failed to get installed package info: "
                            + installedDriverInfo.error().message());
    }

    if (!installedDriverInfo->first) {
        return LINGLONG_ERR("NVIDIA driver package is not installed");
    }

    LogD("{} current version:{}, latest version {}",
         packageName,
         installedDriverInfo->second.packageVersion,
         upgradableDriverInfo->second.packageVersion);

    return compareVersions(upgradableDriverInfo->second.packageVersion,
                           installedDriverInfo->second.packageVersion);
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

utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
NVIDIADriverDetector::getInstalledGraphicsDriverInfo(const std::string &packageName) const
{
    using json = nlohmann::json;

    LINGLONG_TRACE("Get installed NVIDIA graphics driver info");

    GraphicsDriverInfo driverInfo{ getDriverIdentify(), packageName };

    auto listResult = linglong::utils::Cmd("ll-cli").exec({ "--json", "list", "--type=extension" });
    if (!listResult) {
        return LINGLONG_ERR(listResult);
    }

    bool installed = false;
    try {
        json installedExtensions = json::parse(*listResult);
        if (!installedExtensions.is_array()) {
            return LINGLONG_ERR("Invalid list result JSON: expected an array");
        }

        for (const auto &item : installedExtensions) {
            if (!item.is_object())
                continue;

            auto idIter = item.find("id");
            if (idIter == item.end() || !idIter->is_string()
                || idIter->get<std::string_view>() != packageName) {
                continue;
            }

            auto versionIter = item.find("version");
            if (versionIter == item.end() || !versionIter->is_string()) {
                return LINGLONG_ERR("Installed package found but version field is missing: "
                                    + packageName);
            }

            driverInfo.packageVersion = versionIter->get<std::string_view>();
            installed = true;
            break;
        }
    } catch (const nlohmann::json::parse_error &e) {
        return LINGLONG_ERR("Failed to parse installed package JSON: " + std::string(e.what()));
    }

    return std::make_pair(installed, driverInfo);
}

bool NVIDIADriverDetector::compareVersions(std::string_view v1, std::string_view v2) const noexcept
{
    auto ver1 = package::Version::parse(std::string(v1));
    auto ver2 = package::Version::parse(std::string(v2));

    if (!ver1 || !ver2) {
        LogW("Failed to parse versions: {} , {}", v1, v2);
        return false;
    }

    return v1 > v2;
}

} // namespace linglong::driver::detect
