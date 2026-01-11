/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/runtime/run_context.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linglong::cli::extension_override {

std::optional<std::filesystem::path> getUserConfigPath() noexcept;

utils::error::Result<std::vector<runtime::ExtensionOverride>>
loadOverrides(const std::filesystem::path &configPath) noexcept;

utils::error::Result<void> importCdiOverrides(const std::filesystem::path &configPath,
                                              const std::optional<std::filesystem::path> &cdiPath,
                                              const std::string &name,
                                              bool fallbackOnly) noexcept;

} // namespace linglong::cli::extension_override
