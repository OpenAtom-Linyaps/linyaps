// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/dir.h"

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
