// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/cdi_drv/types/CdiDrvConfig.hpp"
#include "linglong/utils/error/error.h"
#include "requests.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace linglong::cdi_drv {

struct WatchOptions
{
    std::filesystem::path unitDir{ "/run/systemd/system" };
    std::string systemctl{ "systemctl" };
};

// Configuration names mapped to their generated systemd path unit contents.
using WatchFiles = std::map<std::string, std::string>;

utils::error::Result<std::optional<std::string>> generateWatch(const std::string &name,
                                                               const types::CdiDrvConfig &config);
utils::error::Result<void> syncWatches(const WatchFiles &watches,
                                       const RefreshRequests &requests,
                                       const WatchOptions &options);

} // namespace linglong::cdi_drv
