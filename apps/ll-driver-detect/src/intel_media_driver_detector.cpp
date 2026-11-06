// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "intel_media_driver_detector.h"

#include <string_view>

#include "linglong/package/version.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>

namespace linglong::driver::detect {

IntelMediaDriverDetector::IntelMediaDriverDetector(
  std::vector<std::filesystem::path> intelGpuModulePaths)
    : intelGpuModulePaths_(std::move(intelGpuModulePaths))
{
}

utils::error::Result<GraphicsDriverInfo> IntelMediaDriverDetector::detect()
{
    LINGLONG_TRACE("Intel media driver detection started");

    // Without an Intel GPU there is no reason to talk to the repository at all, so
    // bail out before running any ll-cli command.
    if (!isIntelGpuPresent()) {
        return LINGLONG_ERR("No Intel GPU found on the host");
    }

    const std::string packageName = getDriverIdentify();

    // Check if package exists in repo
    auto packageInfoResult = getPackageInfoFromRemoteRepo(packageName);
    if (!packageInfoResult) {
        return LINGLONG_ERR("Failed to get package info from remote repo: "
                            + packageInfoResult.error().message());
    }

    auto installedResult = checkPackageInstalled(packageName);
    if (!installedResult) {
        return LINGLONG_ERR("Failed to check if package is installed: "
                            + installedResult.error().message());
    }

    if (*installedResult) {
        if (!packageInfoResult->first) {
            LogW("Intel media driver package is installed locally but not found in remote repo: {}",
                 packageName);
            return LINGLONG_ERR("Cannot find Intel media driver package in remote repo.");
        }

        auto upgradableResult = checkPackageUpgradable(packageName);
        if (!upgradableResult) {
            return LINGLONG_ERR("Failed to check if package is upgradable: "
                                + upgradableResult.error().message());
        }
        if (*upgradableResult) {
            LogD("Intel media driver package can be upgraded: {}, install from remote repo.",
                 packageName);
            return packageInfoResult->second;
        }
        return LINGLONG_ERR("Intel media driver package is already installed and up-to-date.");
    }

    if (!packageInfoResult->first) {
        return LINGLONG_ERR("Intel media driver package not found in remote repo");
    }

    LogD("Intel media driver package is not installed, install from remote repo.");
    return packageInfoResult->second;
}

utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
IntelMediaDriverDetector::getPackageInfoFromRemoteRepo(const std::string &packageName)
{
    using json = nlohmann::json;

    LINGLONG_TRACE("Check if Intel media driver package exists in repository");

    GraphicsDriverInfo driverInfo;
    driverInfo.identify = getDriverIdentify();
    driverInfo.packageName = packageName;

    // Execute ll-cli search command to look for the driver package. The result is a
    // map from repository alias to the packages found in that repository.
    auto ret = linglong::utils::Cmd("ll-cli").exec({ "--json", "search", packageName });
    if (!ret) {
        return LINGLONG_ERR("Search command failed: " + ret.error().message());
    }

    bool found = false;

    try {
        json j = json::parse(*ret);

        for (const auto &[repoAlias, packages] : j.items()) {
            if (!packages.is_array())
                continue;

            for (const auto &item : packages) {
                if (!item.is_object())
                    continue;

                // The search API performs a fuzzy match, so unrelated packages can
                // show up here. Only accept the exact identify we are looking for.
                auto idIter = item.find("id");
                if (idIter == item.end() || !idIter->is_string()
                    || idIter->get<std::string_view>() != packageName) {
                    continue;
                }

                auto versionIter = item.find("version");
                if (versionIter == item.end() || !versionIter->is_string())
                    continue;

                std::string_view currentVersionStr = versionIter->get<std::string_view>();

                // A package may be published in several repositories; keep the newest
                // version and remember which repository provides it.
                if (!found || compareVersions(currentVersionStr, driverInfo.packageVersion)) {
                    driverInfo.repoName = repoAlias;
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
IntelMediaDriverDetector::checkPackageInstalled(const std::string &packageName)
{
    LINGLONG_TRACE("Check if Intel media driver package is installed");

    auto installedInfo = getInstalledGraphicsDriverInfo(packageName);
    if (!installedInfo) {
        return LINGLONG_ERR(installedInfo);
    }

    return installedInfo->first;
}

utils::error::Result<bool>
IntelMediaDriverDetector::checkPackageUpgradable(const std::string &packageName)
{
    LINGLONG_TRACE("Check if Intel media driver package is upgradable");

    auto upgradableDriverInfo = getPackageInfoFromRemoteRepo(packageName);
    if (!upgradableDriverInfo) {
        return LINGLONG_ERR("Failed to get upgradable package info: "
                            + upgradableDriverInfo.error().message());
    }

    if (!upgradableDriverInfo->first) {
        return LINGLONG_ERR("Intel media driver package not found in remote repo");
    }

    auto installedDriverInfo = getInstalledGraphicsDriverInfo(packageName);
    if (!installedDriverInfo) {
        return LINGLONG_ERR("Failed to get installed package info: "
                            + installedDriverInfo.error().message());
    }

    if (!installedDriverInfo->first) {
        return LINGLONG_ERR("Intel media driver package is not installed");
    }

    LogD("{} current version:{}, latest version {}",
         packageName,
         installedDriverInfo->second.packageVersion,
         upgradableDriverInfo->second.packageVersion);

    return compareVersions(upgradableDriverInfo->second.packageVersion,
                           installedDriverInfo->second.packageVersion);
}

bool IntelMediaDriverDetector::isIntelGpuPresent() const
{
    // The i915 and xe kernel modules both expose an Intel GPU to userspace; the
    // presence of either module directory is what we treat as "Intel GPU present".
    return std::any_of(intelGpuModulePaths_.begin(),
                       intelGpuModulePaths_.end(),
                       [](const std::filesystem::path &modulePath) {
                           return std::filesystem::exists(modulePath);
                       });
}

utils::error::Result<std::pair<bool, GraphicsDriverInfo>>
IntelMediaDriverDetector::getInstalledGraphicsDriverInfo(const std::string &packageName) const
{
    using json = nlohmann::json;

    LINGLONG_TRACE("Get installed Intel media graphics driver info");

    GraphicsDriverInfo driverInfo;
    driverInfo.identify = getDriverIdentify();
    driverInfo.packageName = packageName;

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

bool IntelMediaDriverDetector::compareVersions(std::string_view v1, std::string_view v2) const noexcept
{
    auto ver1 = package::Version::parse(std::string(v1));
    auto ver2 = package::Version::parse(std::string(v2));

    if (!ver1 || !ver2) {
        LogW("Failed to parse versions: {} , {}", v1, v2);
        return false;
    }

    return *ver1 > *ver2;
}

} // namespace linglong::driver::detect
