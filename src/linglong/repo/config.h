/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_REPO_CONFIG_H_
#define LINGLONG_REPO_CONFIG_H_

#include "linglong/api/types/helper.h"
#include "linglong/api/types/v1/RepoConfig.hpp"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::repo {

utils::error::Result<api::types::v1::RepoConfig> loadConfig(const QString &file) noexcept;
utils::error::Result<api::types::v1::RepoConfig> loadConfig(const QStringList &files) noexcept;
utils::error::Result<void> saveConfig(const api::types::v1::RepoConfig &cfg,
                                      const QString &path) noexcept;

} // namespace linglong::repo
#endif
