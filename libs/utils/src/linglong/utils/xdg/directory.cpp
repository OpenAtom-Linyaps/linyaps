// SPDX-FileCopyrightText: 2024 - 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/xdg/directory.h"

namespace linglong::utils::xdg {

utils::error::Result<std::filesystem::path> appDataDir(const std::string &appID) noexcept
{
    LINGLONG_TRACE("get app data dir");
    auto *home = std::getenv("HOME"); // NOLINT
    if (home == nullptr) {
        return LINGLONG_ERR("HOME is not set");
    }

    return std::filesystem::path{ home } / ".linglong" / appID;
}

} // namespace linglong::utils::xdg
