// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/xdg.h"

#include <unistd.h>

namespace linglong::common::xdg {

std::filesystem::path getXDGRuntimeDir() noexcept
{
    auto *runtimeDirEnv = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirEnv != nullptr && runtimeDirEnv[0] != '\0') {
        return runtimeDirEnv;
    }

    // fallback to default
    // /tmp/linglong-runtime-$UID
    return std::filesystem::path{ "/tmp" } / ("linglong-runtime-" + std::to_string(::getuid()));
}

std::filesystem::path getXDGCacheHomeDir() noexcept
{
    auto *cacheHomeEnv = std::getenv("XDG_CACHE_HOME");
    if (cacheHomeEnv != nullptr && cacheHomeEnv[0] != '\0') {
        return cacheHomeEnv;
    }

    // fallback to default
    // $HOME/.cache
    auto *homeEnv = std::getenv("HOME");
    if (homeEnv != nullptr && homeEnv[0] != '\0') {
        return std::filesystem::path{ homeEnv } / ".cache";
    }

    return "";
}

} // namespace linglong::common::xdg
