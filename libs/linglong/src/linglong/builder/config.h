/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/utils/error/error.h"

#include <QStandardPaths>
#include <QString>

namespace linglong::builder {

auto loadConfig(const QString &file) noexcept
  -> utils::error::Result<api::types::v1::BuilderConfig>;
auto loadConfig(const QStringList &files) noexcept
  -> utils::error::Result<api::types::v1::BuilderConfig>;
auto saveConfig(const api::types::v1::BuilderConfig &cfg, const QString &path) noexcept
  -> utils::error::Result<void>;

} // namespace linglong::builder
