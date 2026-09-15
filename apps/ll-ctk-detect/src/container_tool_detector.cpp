// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "container_tool_detector.h"

#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"

#include <sstream>
#include <string>

namespace linglong::ctk::detect {

namespace {

detail::PackageStatusQuery defaultDpkgQuery()
{
    return [](const std::vector<std::string> &args) -> utils::error::Result<std::string> {
        LINGLONG_TRACE("query dpkg");

        auto command = linglong::utils::Cmd("dpkg-query");
        return command.exec(args);
    };
}

} // namespace

std::vector<MissingTool> findMissingContainerTools(const std::vector<ContainerToolSpec> &tools,
                                                   const std::set<std::string> &detectedDeviceIds)
{
    return detail::findMissingContainerTools(tools, detectedDeviceIds, defaultDpkgQuery());
}

namespace detail {

std::vector<MissingTool> findMissingContainerTools(const std::vector<ContainerToolSpec> &tools,
                                                   const std::set<std::string> &detectedDeviceIds,
                                                   const PackageStatusQuery &queryDpkg)
{
    LINGLONG_TRACE("find missing container tools")

    auto installedPackagesResult =
      queryDpkg({ "-W", "-f=${db:Status-Abbrev} ${binary:Package}\n" });
    if (!installedPackagesResult) {
        LogE("failed to query installed packages: {}", installedPackagesResult.error().message());
        return {};
    }

    const auto &installedPackages = *installedPackagesResult;
    std::vector<MissingTool> missing;
    for (const auto &tool : tools) {
        if (detectedDeviceIds.find(tool.deviceId) == detectedDeviceIds.end()) {
            LogD("device {} is not detected; skipping tool kit", tool.deviceId);
            continue;
        }

        MissingTool missingTool{ tool.deviceId, {} };
        for (const auto &package : tool.packages) {
            if (isPackageInstalled(package, installedPackages)) {
                LogD("container tool package {} for device {} is already installed",
                     package,
                     tool.deviceId);
                continue;
            }

            LogD("container tool package {} for device {} is missing", package, tool.deviceId);
            missingTool.packages.emplace_back(package);
        }

        if (!missingTool.packages.empty()) {
            missing.emplace_back(std::move(missingTool));
        }
    }

    return missing;
}

bool isPackageInstalled(const std::string &package, const std::string &installedPackages)
{
    std::istringstream output(installedPackages);
    std::string line;
    while (std::getline(output, line)) {
        std::istringstream lineStream(line);
        std::string status;
        std::string packageName;
        lineStream >> status >> packageName;
        if (packageName.empty()) {
            continue;
        }

        const auto architectureSeparator = packageName.rfind(':');
        if (architectureSeparator != std::string::npos) {
            packageName.erase(architectureSeparator);
        }

        if (status.size() == 2 && (status[0] == 'i' || status[0] == 'h') && status[1] == 'i'
            && packageName == package) {
            return true;
        }
    }

    return false;
}

} // namespace detail

} // namespace linglong::ctk::detect
