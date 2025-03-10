// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include "linglong/utils/error/error.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace linglong::utils {
linglong::utils::error::Result<std::string> readFile(std::string filepath);

linglong::utils::error::Result<void> writeFile(const std::string &filepath,
                                               const std::string &content);

linglong::utils::error::Result<uintmax_t>
calculateDirectorySize(const std::filesystem::path &dir) noexcept;
} // namespace linglong::utils
