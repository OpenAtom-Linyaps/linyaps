// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdlib>

namespace linglong::dialog {

enum class PermissionDialogResult {
    Denied,
    Allowed,
};

[[nodiscard]] constexpr int exitCode(PermissionDialogResult result) noexcept
{
    return result == PermissionDialogResult::Allowed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace linglong::dialog
