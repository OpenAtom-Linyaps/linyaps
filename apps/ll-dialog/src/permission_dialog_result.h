// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <cstdlib>

namespace linglong::dialog {

// Protocol result of a permission dialog interaction.
// The dialog protocol (DialogHandShakePayload) requires that only an
// explicit allow exits with status 0; every other outcome is a rejection.
enum class PermissionDialogResult {
    Denied,
    Allowed,
};

[[nodiscard]] constexpr int exitCode(PermissionDialogResult result) noexcept
{
    return result == PermissionDialogResult::Allowed ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace linglong::dialog
