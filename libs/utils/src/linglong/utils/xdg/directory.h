// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>

namespace linglong::utils::xdg {

utils::error::Result<std::filesystem::path> appDataDir(const std::string &appID) noexcept;
} // namespace linglong::utils::xdg
