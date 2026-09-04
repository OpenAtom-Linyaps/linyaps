/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <string>
#include <string_view>

namespace linglong::package {

// Validate that a name is a legal single Linux path component, suitable for use
// as an exported executable name. Rules: non-empty, not "." or "..", contains
// no '/', contains no NUL byte, and length <= 255 (NAME_MAX).
utils::error::Result<void> validateExecutableName(std::string_view name) noexcept;

} // namespace linglong::package
