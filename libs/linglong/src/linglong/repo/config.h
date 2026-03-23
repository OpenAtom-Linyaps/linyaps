/*
 * SPDX-FileCopyrightText: 2022 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>

namespace linglong::repo {

utils::error::Result<api::types::v1::RepoConfigV2>
loadConfig(const std::filesystem::path &file) noexcept;
utils::error::Result<api::types::v1::RepoConfigV2>
loadConfig(const std::vector<std::filesystem::path> &files) noexcept;
utils::error::Result<void> saveConfig(const api::types::v1::RepoConfigV2 &cfg,
                                      const std::filesystem::path &path) noexcept;
int64_t getRepoMinPriority(const api::types::v1::RepoConfigV2 &cfg) noexcept;
int64_t getRepoMaxPriority(const api::types::v1::RepoConfigV2 &cfg) noexcept;
const api::types::v1::Repo &getDefaultRepo(const api::types::v1::RepoConfigV2 &cfg) noexcept;

std::vector<api::types::v1::Repo> getPrioritySortedRepos(api::types::v1::RepoConfigV2 cfg) noexcept;
std::vector<std::vector<api::types::v1::Repo>>
getPriorityGroupedRepos(api::types::v1::RepoConfigV2 cfg) noexcept;

api::types::v1::RepoConfigV2 convertToV2(const api::types::v1::RepoConfig &cfg) noexcept;

} // namespace linglong::repo
