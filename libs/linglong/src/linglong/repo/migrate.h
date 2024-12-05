// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/RepoConfig.hpp"

#include <filesystem>

namespace linglong::repo {
enum class MigrateResult { Success, Failed, NoChange };

MigrateResult tryMigrate(const std::filesystem::path &root,
                         const linglong::api::types::v1::RepoConfig &cfg) noexcept;
} // namespace linglong::repo
