// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/scoped_umask.h"
#include "common/tempdir.h"
#include "linglong/common/constants.h"
#include "linglong/utils/file.h"

#include <filesystem>

namespace fs = std::filesystem;

TEST(DirTest, ContainerCacheDirectoryUsesPackageManagerUmask)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    const auto cacheDir = tempDir.path() / "cache";
    const auto commitDir = cacheDir / "commit";
    const auto containerDir = commitDir / "container";

    ScopedUmask scopedUmask{ 0022 };
    auto result = linglong::utils::ensureDirectory(containerDir);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(fs::status(cacheDir).permissions() & fs::perms::mask,
              linglong::common::shared_directory_permissions);
    EXPECT_EQ(fs::status(commitDir).permissions() & fs::perms::mask,
              linglong::common::shared_directory_permissions);
    EXPECT_EQ(fs::status(containerDir).permissions() & fs::perms::mask,
              linglong::common::shared_directory_permissions);
}
