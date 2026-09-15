// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "container_tool_config.h"
#include "linglong/utils/error/error.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace linglong::ctk::detect {

struct MissingTool
{
    std::string deviceId;
    std::vector<std::string> packages;
};

std::vector<MissingTool> findMissingContainerTools(const std::vector<ContainerToolSpec> &tools,
                                                   const std::set<std::string> &detectedDeviceIds);

namespace detail {

using PackageStatusQuery =
  std::function<utils::error::Result<std::string>(const std::vector<std::string> &args)>;

std::vector<MissingTool> findMissingContainerTools(const std::vector<ContainerToolSpec> &tools,
                                                   const std::set<std::string> &detectedDeviceIds,
                                                   const PackageStatusQuery &queryDpkg);

bool isPackageInstalled(const std::string &package, const std::string &installedPackages);

} // namespace detail

} // namespace linglong::ctk::detect
