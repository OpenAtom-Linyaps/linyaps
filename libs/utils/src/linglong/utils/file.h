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

linglong::utils::error::Result<void>
copyDirectory(const std::filesystem::path &src,
              const std::filesystem::path &dest,
              std::function<bool(const std::filesystem::path &)> matcher,
              std::filesystem::copy_options options = std::filesystem::copy_options::copy_symlinks
                | std::filesystem::copy_options::skip_existing);

} // namespace linglong::utils
