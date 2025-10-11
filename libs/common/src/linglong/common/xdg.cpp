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

} // namespace linglong::common::xdg
