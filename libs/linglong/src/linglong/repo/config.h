/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::repo {

utils::error::Result<api::types::v1::RepoConfigV2> loadConfig(const QString &file) noexcept;
utils::error::Result<api::types::v1::RepoConfigV2> loadConfig(const QStringList &files) noexcept;
utils::error::Result<void> saveConfig(const api::types::v1::RepoConfigV2 &cfg,
                                      const QString &path) noexcept;
std::string getDefaultRepoUrl(const api::types::v1::RepoConfigV2 &cfg) noexcept;
api::types::v1::RepoConfigV2 convertToV2(const api::types::v1::RepoConfig &cfg) noexcept;

} // namespace linglong::repo
