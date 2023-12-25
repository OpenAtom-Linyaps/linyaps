/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_REPO_CONFIG_H_
#define LINGLONG_REPO_CONFIG_H_

#include "linglong/repo/config/ConfigV1.hpp"
#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::repo {

utils::error::Result<config::ConfigV1> loadConfig(const QString &file) noexcept;
utils::error::Result<config::ConfigV1> loadConfig(const QStringList &files) noexcept;
utils::error::Result<void> saveConfig(const config::ConfigV1 &cfg, const QString &path) noexcept;

inline bool operator==(const config::ConfigV1 &cfg1, const config::ConfigV1 &cfg2) noexcept
{
    return cfg1.version == cfg2.version && cfg1.repos == cfg2.repos
      && cfg1.defaultRepo == cfg2.defaultRepo;
}

inline bool operator!=(const config::ConfigV1 &cfg1, const config::ConfigV1 &cfg2) noexcept
{
    return !(cfg1 == cfg2);
}

} // namespace linglong::repo
#endif
