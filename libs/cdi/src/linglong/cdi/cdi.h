/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CdiDeviceEntry.hpp"
#include "linglong/cdi/types/ContainerEdits.hpp"
#include "linglong/utils/error/error.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace linglong::cdi {

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>> getCDIDevices(
  const std::vector<std::string> &specDirs, const std::optional<std::vector<std::string>> &devices);

utils::error::Result<types::ContainerEdits>
getCDIDeviceEdits(const api::types::v1::CdiDeviceEntry &device);

// Resolve a selection together so spec-level edits are applied once per spec file.
utils::error::Result<types::ContainerEdits>
getCDIDevicesEdits(const std::vector<api::types::v1::CdiDeviceEntry> &devices);

} // namespace linglong::cdi
