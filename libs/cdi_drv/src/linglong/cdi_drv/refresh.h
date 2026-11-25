// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "config.h"
#include "linglong/utils/error/error.h"
#include "watch.h"

#include <chrono>
#include <filesystem>
#include <string>

namespace linglong::cdi_drv {

struct RefreshOptions
{
    std::filesystem::path configDir;
    std::filesystem::path runtimeDir;
    std::string name;
    bool pending{ false };
    WatchOptions watches;
    std::chrono::seconds idleTimeout{ 5 };
};

utils::error::Result<void> requestRefresh(const std::filesystem::path &runtimeDir,
                                          const std::string &name);
utils::error::Result<void> refresh(const RefreshOptions &options);

} // namespace linglong::cdi_drv
