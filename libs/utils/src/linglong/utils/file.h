// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <functional>
#include <string>

namespace linglong::utils {

linglong::utils::error::Result<std::string> readFile(const std::filesystem::path &filepath);

linglong::utils::error::Result<void> writeFile(const std::filesystem::path &filepath,
                                               const std::string &content);
linglong::utils::error::Result<void> writeFile(const std::filesystem::path &filepath,
                                               const std::string &content,
                                               std::filesystem::perms permissions);

linglong::utils::error::Result<void> concatFile(const std::filesystem::path &source,
                                                const std::filesystem::path &target);

linglong::utils::error::Result<uintmax_t>
calculateDirectorySize(const std::filesystem::path &dir) noexcept;

// Merge matching entries into dest without following symlinks. Special files are skipped.
// Unmatched directories are still traversed so their matching descendants can be copied.
// Existing files and symlinks are kept; existing directories are merged.
[[nodiscard]] linglong::utils::error::Result<void>
copyDirectory(const std::filesystem::path &src,
              const std::filesystem::path &dest,
              std::function<bool(const std::filesystem::path &)> matcher = {});

linglong::utils::error::Result<void>
moveFiles(const std::filesystem::path &src,
          const std::filesystem::path &dest,
          std::function<bool(const std::filesystem::path &)> matcher);

linglong::utils::error::Result<std::vector<std::filesystem::path>>
getFiles(const std::filesystem::path &dir);

linglong::utils::error::Result<void> ensureDirectory(const std::filesystem::path &dir);
linglong::utils::error::Result<void> ensureDirectory(const std::filesystem::path &dir,
                                                     std::filesystem::perms permissions);

linglong::utils::error::Result<void>
makeDirectoryTreeRemovable(const std::filesystem::path &root) noexcept;

linglong::utils::error::Result<void> relinkFileTo(const std::filesystem::path &link,
                                                  const std::filesystem::path &target) noexcept;

} // namespace linglong::utils
