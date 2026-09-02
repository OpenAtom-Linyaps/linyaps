// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/scoped_umask.h"
#include "common/tempdir.h"
#include "linglong/common/constants.h"
#include "linglong/common/dir.h"
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

namespace linglong::common::dir {

TEST(DirTest, PathInDirectory)
{
    const std::filesystem::path directory{ "/tmp/project" };

    EXPECT_TRUE(isPathInDirectory(directory, directory));
    EXPECT_TRUE(isPathInDirectory("/tmp/project/linglong.yaml", directory));
    EXPECT_TRUE(isPathInDirectory("/tmp/project/linglong.yaml", "/tmp/project/"));
    EXPECT_TRUE(isPathInDirectory("/tmp/project/sub/../linglong.yaml", directory));
    EXPECT_TRUE(isPathInDirectory("linglong.yaml", "."));
    EXPECT_TRUE(isPathInDirectory("project/linglong.yaml", "project/"));
}

TEST(DirTest, PathOutsideDirectory)
{
    const std::filesystem::path directory{ "/tmp/project" };

    EXPECT_FALSE(isPathInDirectory("/tmp/project-sibling/linglong.yaml", directory));
    EXPECT_FALSE(isPathInDirectory("/tmp/other/linglong.yaml", directory));
    EXPECT_FALSE(isPathInDirectory("/tmp/project/../outside/linglong.yaml", directory));
    EXPECT_FALSE(isPathInDirectory("../outside/linglong.yaml", "."));
    EXPECT_FALSE(isPathInDirectory("relative/linglong.yaml", directory));
    EXPECT_FALSE(isPathInDirectory({}, directory));
    EXPECT_FALSE(isPathInDirectory("/tmp/project/linglong.yaml", {}));
}

} // namespace linglong::common::dir
