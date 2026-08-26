// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <filesystem>

namespace linglong::common {

constexpr auto default_file_mode = 0644;

constexpr auto shared_directory_permissions = std::filesystem::perms::owner_all
  | std::filesystem::perms::group_read | std::filesystem::perms::group_exec
  | std::filesystem::perms::others_read | std::filesystem::perms::others_exec;

constexpr auto shared_file_permissions = std::filesystem::perms::owner_read
  | std::filesystem::perms::owner_write | std::filesystem::perms::group_read
  | std::filesystem::perms::others_read;

} // namespace linglong::common
