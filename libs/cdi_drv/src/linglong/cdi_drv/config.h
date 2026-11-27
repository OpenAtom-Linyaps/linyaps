// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/cdi_drv/types/CdiDrvConfig.hpp"
#include "linglong/utils/error/error.h"
#include "requests.h"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace linglong::cdi_drv {

bool validConfigName(std::string_view name);
utils::error::Result<std::set<std::string>> configNames(const std::filesystem::path &configDir,
                                                        const RefreshRequests &requests);
utils::error::Result<std::optional<types::CdiDrvConfig>>
loadConfig(const std::filesystem::path &path);

} // namespace linglong::cdi_drv
