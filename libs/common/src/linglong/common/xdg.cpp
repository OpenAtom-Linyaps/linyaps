// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/xdg.h"

#include <unistd.h>

namespace linglong::common {

std::filesystem::path getXDGRuntimeDir() noexcept
{
    auto *runtimeDirEnv = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirEnv != nullptr && runtimeDirEnv[0] != '\0') {
        return runtimeDirEnv;
    }

    // fallback to default
    return std::filesystem::path{ "/tmp" } / "linglong" / std::to_string(::getuid());
}

std::filesystem::path getAppXDGRuntimeDir(const std::string &appId) noexcept
{
    return getXDGRuntimeDir() / "linglong/apps" / appId;
}

} // namespace linglong::common
