// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "permission_dialog_result.h"

namespace linglong::dialog::test {

TEST(PermissionDialogResult, AllowsOnlyExplicitPermission)
{
    EXPECT_EQ(exitCode(PermissionDialogResult::Allowed), EXIT_SUCCESS);
    EXPECT_NE(exitCode(PermissionDialogResult::Denied), EXIT_SUCCESS);
    EXPECT_NE(exitCode(static_cast<PermissionDialogResult>(-1)), EXIT_SUCCESS);
}

} // namespace linglong::dialog::test
