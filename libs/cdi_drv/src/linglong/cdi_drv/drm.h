// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <set>
#include <string>

namespace linglong::cdi_drv {

namespace types {
struct DrmSelector;
}

utils::error::Result<std::set<std::string>> drmNodes(const types::DrmSelector &selector);

utils::error::Result<std::set<std::string>>
drmNodesSysfs(const types::DrmSelector &selector,
              const std::filesystem::path &classDirectory = "/sys/class/drm",
              const std::filesystem::path &deviceDirectory = "/dev/dri");

} // namespace linglong::cdi_drv
