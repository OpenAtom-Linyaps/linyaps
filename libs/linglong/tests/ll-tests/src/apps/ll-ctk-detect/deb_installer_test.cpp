// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "deb_installer.h"

#include <filesystem>

using namespace linglong::ctk::detect;

TEST(DebInstallerTest, CreateTempDownloadDirIsPrivate)
{
    auto result = createTempDownloadDir();
    ASSERT_TRUE(result) << result.error().message();

    const auto &dir = *result;
    ASSERT_TRUE(std::filesystem::exists(dir));

    const auto permissions = std::filesystem::status(dir).permissions();
    EXPECT_EQ(permissions & std::filesystem::perms::group_all, std::filesystem::perms::none);
    EXPECT_EQ(permissions & std::filesystem::perms::others_all, std::filesystem::perms::none);

    cleanupTempDir(dir);
    EXPECT_FALSE(std::filesystem::exists(dir));
}
