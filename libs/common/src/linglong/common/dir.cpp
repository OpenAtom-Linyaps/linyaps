// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dir.h"

#include "configure.h"
#include "linglong/common/xdg.h"

namespace linglong::common::dir {

std::filesystem::path getRuntimeDir() noexcept
{
    return xdg::getXDGRuntimeDir() / "linglong";
}

std::filesystem::path getAppRuntimeDir(const std::string &appId) noexcept
{
    return getRuntimeDir() / "apps" / appId;
}

std::filesystem::path getBundleDir(const std::string &containerId) noexcept
{
    return getRuntimeDir() / containerId;
}

std::filesystem::path getUserCacheDir() noexcept
{
    auto cacheDir = xdg::getXDGCacheHomeDir();
    // If neither XDG_CACHE_HOME nor HOME is set, use LINGLONG_ROOT/cache as fallback,
    // normally it's can only be written by the package manager
    if (cacheDir.empty()) {
        return std::filesystem::path{ LINGLONG_ROOT } / "cache";
    }

    return cacheDir / "linglong";
}

std::filesystem::path getUserRuntimeConfigDir() noexcept
{
    auto configDir = xdg::getXDGConfigHomeDir();
    // If neither XDG_CONFIG_HOME nor HOME is set, return empty path
    if (configDir.empty()) {
        return {};
    }

    return configDir / "linglong";
}

} // namespace linglong::common::dir
