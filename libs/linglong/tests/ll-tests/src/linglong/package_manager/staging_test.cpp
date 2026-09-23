// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/package_manager/package_manager.h"

#include <filesystem>
#include <fstream>

using namespace linglong::service;

TEST(StagingCleanupTest, RemovesOnlyTheCurrentPackageAndUnpackDirectory)
{
    TempDir stagingDir("linglong-staging-");
    ASSERT_TRUE(stagingDir.isValid());

    const auto stagedFile = stagingDir.path() / "install-current";
    std::ofstream{ stagedFile } << "current package";
    auto unpackDir = stagedFile;
    unpackDir += ".unpack";
    std::filesystem::create_directories(unpackDir / "unpack");
    std::ofstream{ unpackDir / "unpack" / "marker" } << "current extraction";

    const auto siblingStagedFile = stagingDir.path() / "install-sibling";
    std::ofstream{ siblingStagedFile } << "another active install";

    auto result = detail::cleanStagingArtifact(stagingDir.path(), stagedFile);

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(std::filesystem::exists(stagedFile));
    EXPECT_FALSE(std::filesystem::exists(unpackDir));
    EXPECT_TRUE(std::filesystem::exists(siblingStagedFile));
    EXPECT_TRUE(std::filesystem::exists(stagingDir.path()));
}

TEST(StagingCleanupTest, RejectsPathsOutsideTheStagingDirectory)
{
    TempDir stagingDir("linglong-staging-");
    TempDir outsideDir("linglong-outside-staging-");
    ASSERT_TRUE(stagingDir.isValid());
    ASSERT_TRUE(outsideDir.isValid());
    const auto outsideFile = outsideDir.path() / "install-not-owned";
    std::ofstream{ outsideFile } << "do not remove";

    auto result = detail::cleanStagingArtifact(stagingDir.path(), outsideFile);

    ASSERT_FALSE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(outsideFile));
}
