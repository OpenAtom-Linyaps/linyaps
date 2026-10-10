/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "configure.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>

namespace linglong::builder {

auto findBuilderScript(const std::string &scriptName) noexcept
  -> utils::error::Result<std::filesystem::path>;
auto findBuilderTemplate() noexcept -> utils::error::Result<std::filesystem::path>;

namespace detail {

auto isInstalledExecutable(const std::filesystem::path &executable,
                           const std::filesystem::path &installedBinDir) noexcept -> bool;

auto findBuilderFileForExecutable(const std::filesystem::path &executable,
                                  const std::filesystem::path &buildRelative,
                                  const std::filesystem::path &installed,
                                  const std::filesystem::path &installedBinDir = BINDIR) noexcept
  -> utils::error::Result<std::filesystem::path>;

} // namespace detail

} // namespace linglong::builder
