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
#include <string>
#include <vector>

namespace linglong::cdi {

utils::error::Result<std::vector<api::types::v1::CdiDeviceEntry>>
getCDIDevices(const std::vector<std::string> &specDirs, const std::vector<std::string> &devices);

utils::error::Result<types::ContainerEdits>
getCDIDeviceEdits(const api::types::v1::CdiDeviceEntry &device);

} // namespace linglong::cdi
