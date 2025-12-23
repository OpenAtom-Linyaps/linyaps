// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/file.h"
#include "linglong/utils/global/initialize.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class FileTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        char src_template[] = "/tmp/linglong-file-test-src-XXXXXX";
        src_dir = mkdtemp(src_template);
        ASSERT_FALSE(src_dir.empty());

        char dest_template[] = "/tmp/linglong-file-test-dest-XXXXXX";
        dest_dir = mkdtemp(dest_template);
        ASSERT_FALSE(dest_dir.empty());

        fs::create_directories(src_dir / "subdir1" / "subdir2");

        std::ofstream(src_dir / "file1.txt") << "content1";
        std::ofstream(src_dir / "subdir1" / "file2.txt") << "content2";
        std::ofstream(src_dir / "subdir1" / "subdir2" / "file3.txt") << "content3";
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

    linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content1");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
}

TEST_F(FileTest, CopyDirectory_MatcherFileInUnmatchedDir)
{
    auto matcher = [](const fs::path &path) {
        // only match file2.txt, not its parent subdir1
        return path.filename() == "file2.txt";
    };

    linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    EXPECT_FALSE(fs::exists(dest_dir / "file1.txt"));
    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));
    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));

    std::ifstream ifs(dest_dir / "subdir1" / "file2.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content2");
}

TEST_F(FileTest, CopyDirectory_MatcherSubDirInUnmatchedDir)
{
    auto matcher = [](const fs::path &path) {
        // only match subdir2 and its contents, not its parent subdir1
        return path.string().rfind("subdir1/subdir2", 0) == 0;
    };

    linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    EXPECT_FALSE(fs::exists(dest_dir / "file1.txt"));
    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
    EXPECT_FALSE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1" / "subdir2"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "subdir2" / "file3.txt"));

    std::ifstream ifs(dest_dir / "subdir1" / "subdir2" / "file3.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content3");
}

TEST_F(FileTest, CopyDirectory_OverwriteExisting)
{
    std::ofstream(dest_dir / "file1.txt") << "existing_content";

    auto matcher = [](const fs::path &path) {
        return path.filename() != "ignored.txt";
    };

    linglong::utils::copyDirectory(src_dir,
                                   dest_dir,
                                   matcher,
                                   fs::copy_options::overwrite_existing
                                     | fs::copy_options::copy_symlinks);
    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
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

    linglong::utils::copyDirectory(src_dir, dest_dir, matcher);
    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "existing_content");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
}

TEST_F(FileTest, MoveFiles)
{
    auto matcher = [](const fs::path &path) {
        return path.filename() != "ignored.txt";
    };

    auto result = linglong::utils::moveFiles(src_dir, dest_dir, matcher);
    ASSERT_TRUE(result.has_value());

    // Check files in destination
    EXPECT_TRUE(fs::exists(dest_dir / "file1.txt"));
    std::ifstream ifs(dest_dir / "file1.txt");
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    EXPECT_EQ(content, "content1");

    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));
    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1" / "subdir2"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "subdir2" / "file3.txt"));

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink1"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink1"), "file1.txt");

    EXPECT_TRUE(fs::is_symlink(dest_dir / "symlink_abs"));
    EXPECT_EQ(fs::read_symlink(dest_dir / "symlink_abs"), fs::absolute(src_dir / "file1.txt"));

    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));

    // Check files are removed from source
    EXPECT_FALSE(fs::exists(src_dir / "file1.txt"));
    EXPECT_FALSE(fs::exists(src_dir / "symlink1"));
    EXPECT_FALSE(fs::exists(src_dir / "symlink_abs"));
    EXPECT_FALSE(fs::exists(src_dir / "subdir1"));

    // Check ignored file is still in source
    EXPECT_TRUE(fs::exists(src_dir / "ignored.txt"));
}

TEST_F(FileTest, MoveFiles_MatcherFileInUnmatchedDir)
{
    auto matcher = [](const fs::path &path) {
        // only match file2.txt, not its parent subdir1
        return path.filename() == "file2.txt";
    };

    auto result = linglong::utils::moveFiles(src_dir, dest_dir, matcher);
    ASSERT_TRUE(result.has_value());

    // check destination
    EXPECT_FALSE(fs::exists(dest_dir / "file1.txt"));
    EXPECT_FALSE(fs::exists(dest_dir / "ignored.txt"));
    EXPECT_TRUE(fs::exists(dest_dir / "subdir1" / "file2.txt"));
    EXPECT_TRUE(fs::is_directory(dest_dir / "subdir1"));
    EXPECT_FALSE(fs::exists(dest_dir / "subdir1" / "subdir2"));

    // check source
    EXPECT_TRUE(fs::exists(src_dir / "file1.txt"));
    EXPECT_TRUE(fs::exists(src_dir / "ignored.txt"));
    EXPECT_TRUE(fs::is_directory(src_dir / "subdir1"));
    EXPECT_FALSE(fs::exists(src_dir / "subdir1" / "file2.txt"));
    EXPECT_TRUE(fs::exists(src_dir / "subdir1" / "subdir2" / "file3.txt"));
    EXPECT_TRUE(fs::is_directory(src_dir / "subdir1" / "subdir2"));
}

TEST_F(FileTest, GetFiles)
{
    auto result = linglong::utils::getFiles(src_dir);
    ASSERT_TRUE(result.has_value());

    auto files = *result;
    std::sort(files.begin(), files.end());

    std::vector<fs::path> expected_files = { "file1.txt",       "ignored.txt",
                                             "subdir1",         "subdir1/file2.txt",
                                             "subdir1/subdir2", "subdir1/subdir2/file3.txt",
                                             "symlink1",        "symlink_abs" };
    std::sort(expected_files.begin(), expected_files.end());

    EXPECT_EQ(files, expected_files);
}

TEST_F(FileTest, EnsureDirectory)
{
    // Test creating a new directory
    fs::path new_dir = dest_dir / "new_dir";
    auto result = linglong::utils::ensureDirectory(new_dir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fs::is_directory(new_dir));

    // Test with an existing directory
    result = linglong::utils::ensureDirectory(new_dir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fs::is_directory(new_dir));

    // Test with an existing file
    fs::path file_path = dest_dir / "file.txt";
    std::ofstream(file_path) << "test";
    result = linglong::utils::ensureDirectory(file_path);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(fs::is_directory(file_path));

    // Test with mutiple layer directory
    fs::path multiple_dir = dest_dir / "multiple_dir" / "sub_dir";
    result = linglong::utils::ensureDirectory(multiple_dir);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(fs::is_directory(multiple_dir));
}

TEST_F(FileTest, RelinkFile)
{
    fs::path target = dest_dir / "target_file";
    std::ofstream(target) << "target content";

    fs::path link = dest_dir / "link_file";

    // Case 1: link does not exist
    {
        auto result = linglong::utils::relinkFileTo(link, target);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(fs::is_symlink(link));
        EXPECT_EQ(fs::read_symlink(link), target);
    }

    // Case 2: link exists as a regular file
    {
        fs::remove(link);
        std::ofstream(link) << "old content";

        auto result = linglong::utils::relinkFileTo(link, target);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(fs::is_symlink(link));
        EXPECT_EQ(fs::read_symlink(link), target);
    }

    // Case 3: link exists as a symlink
    {
        fs::remove(link);
        fs::path old_target = dest_dir / "old_target";
        fs::create_symlink(old_target, link);

        auto result = linglong::utils::relinkFileTo(link, target);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(fs::is_symlink(link));
        EXPECT_EQ(fs::read_symlink(link), target);
    }

    // Case 4: link exists as a directory
    {
        fs::remove(link);
        fs::create_directories(link / "non-empty");

        auto result = linglong::utils::relinkFileTo(link, target);
        ASSERT_TRUE(result.has_value()) << result.error().message();
        EXPECT_TRUE(fs::is_symlink(link));
        EXPECT_EQ(fs::read_symlink(link), target);
    }
}
