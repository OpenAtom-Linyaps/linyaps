// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/file.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class FileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        src_dir = fs::temp_directory_path() / "copyDirectory_test_src";
        dest_dir = fs::temp_directory_path() / "copyDirectory_test_dest";

        fs::remove_all(src_dir);
        fs::remove_all(dest_dir);

        fs::create_directories(src_dir / "subdir1");
        fs::create_directories(dest_dir);

        std::ofstream(src_dir / "file1.txt") << "content1";
        std::ofstream(src_dir / "subdir1" / "file2.txt") << "content2";
        std::ofstream(src_dir / "ignored.txt") << "ignored";
        fs::create_symlink("file1.txt", src_dir / "symlink1");
        fs::create_symlink(fs::absolute(src_dir / "file1.txt"), src_dir / "symlink_abs");
    }

    void TearDown() override
    {
        fs::remove_all(src_dir);
        fs::remove_all(dest_dir);
    }

    fs::path src_dir;
    fs::path dest_dir;
};

TEST_F(FileTest, CopyDirectory)
{
    auto matcher = [](const fs::path &path) {
        return path.filename() != "ignored.txt";
    };

    auto result = linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content1");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
}

TEST_F(FileTest, CopyDirectory_OverwriteExisting)
{
    std::ofstream(dest_dir / "file1.txt") << "existing_content";

    auto matcher = [](const fs::path &path) {
        return path.filename() != "ignored.txt";
    };

    auto result = linglong::utils::copyDirectory(src_dir,
                                               dest_dir,
                                               matcher,
                                               fs::copy_options::overwrite_existing
                                                 | fs::copy_options::copy_symlinks);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content1");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
}

TEST_F(FileTest, CopyDirectory_DestinationExists)
{
    std::ofstream(dest_dir / "file1.txt") << "existing_content";

    auto matcher = [](const fs::path &path) {
        return path.filename() != "ignored.txt";
    };

    auto result = linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "existing_content");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
}
