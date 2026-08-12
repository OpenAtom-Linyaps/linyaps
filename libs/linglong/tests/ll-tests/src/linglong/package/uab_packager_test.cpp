// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/package/uab_packager.h"

#include <filesystem>
#include <fstream>

namespace linglong::package {
namespace {

TEST(UABPackagerTest, CopyDirectoryForDistributedBundleCreatesHardLinks)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto source = tempDir.path() / "source";
    const auto destination = tempDir.path() / "destination";
    std::filesystem::create_directories(source / "usr/bin");

    const auto sourceFile = source / "usr/bin/program";
    std::ofstream{ sourceFile } << "content";

    auto result = detail::copyDirectoryForDistributedBundle(source, destination);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::is_directory(destination / "usr/bin"));
    EXPECT_TRUE(std::filesystem::equivalent(sourceFile, destination / "usr/bin/program"));
}

TEST(UABPackagerTest, CopyDirectoryForDistributedBundlePreservesSymlinks)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto source = tempDir.path() / "source";
    const auto destination = tempDir.path() / "destination";
    std::filesystem::create_directories(source / "usr/bin");
    std::ofstream{ source / "usr/bin/program" } << "content";
    std::filesystem::create_symlink("program", source / "usr/bin/program-link");
    std::filesystem::create_directory_symlink("usr/bin", source / "bin");

    auto result = detail::copyDirectoryForDistributedBundle(source, destination);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::is_symlink(destination / "usr/bin/program-link"));
    EXPECT_EQ(std::filesystem::read_symlink(destination / "usr/bin/program-link"), "program");

    EXPECT_TRUE(std::filesystem::is_symlink(destination / "bin"));
    EXPECT_EQ(std::filesystem::read_symlink(destination / "bin"), "usr/bin");
}

} // namespace
} // namespace linglong::package
