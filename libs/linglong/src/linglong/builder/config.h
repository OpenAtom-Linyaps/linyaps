/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/BuilderConfig.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>

namespace linglong::builder {

utils::error::Result<api::types::v1::BuilderConfig>
initDefaultBuildConfig(const std::filesystem::path &path);
auto loadConfig(const std::filesystem::path &file) noexcept
  -> utils::error::Result<api::types::v1::BuilderConfig>;
auto loadConfig() noexcept -> utils::error::Result<api::types::v1::BuilderConfig>;
auto saveConfig(const api::types::v1::BuilderConfig &cfg,
                const std::filesystem::path &path) noexcept -> utils::error::Result<void>;

} // namespace linglong::builder
