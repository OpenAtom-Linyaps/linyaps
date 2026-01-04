// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <tl/expected.hpp>

#include <filesystem>

namespace linglong::common::xdg {

std::filesystem::path getXDGRuntimeDir() noexcept;

std::filesystem::path getXDGCacheHomeDir() noexcept;

} // namespace linglong::common::xdg
