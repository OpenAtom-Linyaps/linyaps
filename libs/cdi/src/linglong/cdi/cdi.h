/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/CdiDeviceEntry.hpp"
#include "linglong/cdi/types/Cdi.hpp"
#include "linglong/cdi/types/ContainerEdits.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace linglong::cdi {

bool validKind(std::string_view kind);

bool validDeviceName(std::string_view name);

utils::error::Result<void> validateCDISpec(const types::Cdi &spec);

utils::error::Result<types::Cdi> parseCDISpec(const std::string &content);

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>> getCDIDevices(
  const std::vector<std::string> &specDirs, const std::optional<std::vector<std::string>> &devices);

// The caller must supply a path to a regular CDI spec file.
utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>>
getCDIDevicesFromSpec(const std::filesystem::path &path);

utils::error::Result<types::ContainerEdits>
getCDIDeviceEdits(const api::types::v1::CdiDeviceEntry &device);

} // namespace linglong::cdi
