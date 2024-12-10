// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <cstdlib>
#include <string>

namespace linglong::utils::xdg {

inline utils::error::Result<std::string> appDataDir(const std::string &appID)
{
    LINGLONG_TRACE("get app data dir");
    auto *home = std::getenv("HOME"); // NOLINT
    if (home == nullptr) {
        return LINGLONG_ERR("HOME is not set");
    }

    return std::string(home) + "/.linglong/" + appID;
}

} // namespace linglong::utils::xdg
