// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>

namespace linglong::utils {

linglong::utils::error::Result<std::string> readFile(std::string filepath);

linglong::utils::error::Result<void> writeFile(const std::string &filepath,
                                               const std::string &content);

linglong::utils::error::Result<uintmax_t>
calculateDirectorySize(const std::filesystem::path &dir) noexcept;

void copyDirectory(
  const std::filesystem::path &src,
  const std::filesystem::path &dest,
  std::function<bool(const std::filesystem::path &)> matcher = {},
  std::filesystem::copy_options options = std::filesystem::copy_options::copy_symlinks
    | std::filesystem::copy_options::skip_existing);

linglong::utils::error::Result<void>
moveFiles(const std::filesystem::path &src,
          const std::filesystem::path &dest,
          std::function<bool(const std::filesystem::path &)> matcher);

linglong::utils::error::Result<std::vector<std::filesystem::path>>
getFiles(const std::filesystem::path &dir);

linglong::utils::error::Result<void> ensureDirectory(const std::filesystem::path &dir);

linglong::utils::error::Result<void> relinkFileTo(const std::filesystem::path &link,
                                                  const std::filesystem::path &target) noexcept;

} // namespace linglong::utils
