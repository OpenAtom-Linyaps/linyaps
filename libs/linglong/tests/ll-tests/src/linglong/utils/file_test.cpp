// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/file.h"

#include <algorithm>
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

TEST_F(FileTest, WriteFile)
{
    // Test writing to a new file
    fs::path test_file = dest_dir / "test_write.txt";
    std::string content = "Hello, World!";

    auto result = linglong::utils::writeFile(test_file.string(), content);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(fs::exists(test_file));

    // Verify content was written correctly
    std::ifstream ifs(test_file);
    std::string read_content((std::istreambuf_iterator<char>(ifs)),
                             (std::istreambuf_iterator<char>()));
    EXPECT_EQ(read_content, content);

    // Test overwriting an existing file
    std::string new_content = "New content";
    result = linglong::utils::writeFile(test_file.string(), new_content);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    std::ifstream ifs2(test_file);
    std::string read_content2((std::istreambuf_iterator<char>(ifs2)),
                              (std::istreambuf_iterator<char>()));
    EXPECT_EQ(read_content2, new_content);

    // Test writing empty content
    fs::path empty_file = dest_dir / "empty.txt";
    result = linglong::utils::writeFile(empty_file.string(), "");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_TRUE(fs::exists(empty_file));

    std::ifstream ifs3(empty_file);
    std::string read_content3((std::istreambuf_iterator<char>(ifs3)),
                              (std::istreambuf_iterator<char>()));
    EXPECT_EQ(read_content3, "");

    // Test writing to a file in a subdirectory that doesn't exist
    fs::path subdir_file = dest_dir / "subdir" / "subfile.txt";
    result = linglong::utils::writeFile(subdir_file.string(), "subdir content");
    EXPECT_FALSE(result.has_value()); // Should fail because parent directory doesn't exist
}

TEST_F(FileTest, ReadFile)
{
    // Test reading an existing file
    fs::path test_file = dest_dir / "test_read.txt";
    std::string content = "Test content for reading";
    std::ofstream(test_file) << content;

    auto result = linglong::utils::readFile(test_file.string());
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, content);

    // Test reading an empty file
    fs::path empty_file = dest_dir / "empty_read.txt";
    auto _ = std::ofstream(empty_file); // Create empty file

    auto result_empty = linglong::utils::readFile(empty_file.string());
    ASSERT_TRUE(result_empty.has_value()) << result_empty.error().message();
    EXPECT_EQ(*result_empty, "");

    // Test reading a file with special characters
    fs::path special_file = dest_dir / "special.txt";
    std::string special_content = "Special chars: \n\t\r\"'\\";
    std::ofstream(special_file) << special_content;

    auto result_special = linglong::utils::readFile(special_file.string());
    ASSERT_TRUE(result_special.has_value()) << result_special.error().message();
    EXPECT_EQ(*result_special, special_content);

    // Test reading a non-existent file
    fs::path non_existent = dest_dir / "non_existent.txt";
    auto result_non_existent = linglong::utils::readFile(non_existent.string());
    EXPECT_FALSE(result_non_existent.has_value());

    // Test reading a file in a subdirectory
    fs::path subdir = dest_dir / "subdir";
    fs::create_directories(subdir);
    fs::path subdir_file = subdir / "subfile.txt";
    std::string subdir_content = "Content in subdirectory";
    std::ofstream(subdir_file) << subdir_content;

    auto result_subdir = linglong::utils::readFile(subdir_file.string());
    ASSERT_TRUE(result_subdir.has_value()) << result_subdir.error().message();
    EXPECT_EQ(*result_subdir, subdir_content);
}

TEST_F(FileTest, WriteFileAndReadFile)
{
    // Test write and read operations together
    fs::path test_file = dest_dir / "test_roundtrip.txt";
    std::string original_content =
      "This is a test for write and read operations.\nMultiple lines.\nSpecial chars: 中文";

    // Write the file
    auto write_result = linglong::utils::writeFile(test_file.string(), original_content);
    ASSERT_TRUE(write_result.has_value()) << write_result.error().message();

    // Read the file back
    auto read_result = linglong::utils::readFile(test_file.string());
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message();

    // Verify round-trip integrity
    EXPECT_EQ(*read_result, original_content);

    // Test with binary content
    fs::path binary_file = dest_dir / "test_binary.txt";
    std::string binary_content = "\x01\x02\x03\xFF\xFE\xFD";

    auto write_binary = linglong::utils::writeFile(binary_file.string(), binary_content);
    ASSERT_TRUE(write_binary.has_value()) << write_binary.error().message();

    auto read_binary = linglong::utils::readFile(binary_file.string());
    ASSERT_TRUE(read_binary.has_value()) << read_binary.error().message();

    EXPECT_EQ(*read_binary, binary_content);
}

TEST_F(FileTest, ConcatFile)
{
    // Test concatenating to a new target file
    fs::path source_file = dest_dir / "source.txt";
    fs::path target_file = dest_dir / "target.txt";
    std::string source_content = "Source content";
    std::string target_content = "Target content";

    // Create source file
    std::ofstream(source_file) << source_content;

    // Create target file with initial content
    std::ofstream(target_file) << target_content;

    // Concatenate source to target
    auto result = linglong::utils::concatFile(source_file, target_file);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // Verify concatenated content
    auto read_result = linglong::utils::readFile(target_file.string());
    ASSERT_TRUE(read_result.has_value()) << read_result.error().message();
    EXPECT_EQ(*read_result, target_content + source_content);

    // Test concatenating to an empty target file
    fs::path empty_target = dest_dir / "empty_target.txt";
    fs::path another_source = dest_dir / "another_source.txt";
    std::string another_content = "Another source content";

    std::ofstream(another_source) << another_content;

    auto result_empty = linglong::utils::concatFile(another_source, empty_target);
    ASSERT_TRUE(result_empty.has_value()) << result_empty.error().message();

    auto read_empty = linglong::utils::readFile(empty_target.string());
    ASSERT_TRUE(read_empty.has_value()) << read_empty.error().message();
    EXPECT_EQ(*read_empty, another_content);

    // Test multiple concatenations
    fs::path multi_target = dest_dir / "multi_target.txt";
    fs::path source1 = dest_dir / "source1.txt";
    fs::path source2 = dest_dir / "source2.txt";

    std::ofstream(multi_target) << "Initial";
    std::ofstream(source1) << "First";
    std::ofstream(source2) << "Second";

    auto result1 = linglong::utils::concatFile(source1, multi_target);
    ASSERT_TRUE(result1.has_value()) << result1.error().message();

    auto result2 = linglong::utils::concatFile(source2, multi_target);
    ASSERT_TRUE(result2.has_value()) << result2.error().message();

    auto read_multi = linglong::utils::readFile(multi_target.string());
    ASSERT_TRUE(read_multi.has_value()) << read_multi.error().message();
    EXPECT_EQ(*read_multi, "InitialFirstSecond");
}

TEST_F(FileTest, ConcatFile_ErrorCases)
{
    // Test concatenating from non-existent source file
    fs::path non_existent_source = dest_dir / "non_existent.txt";
    fs::path target = dest_dir / "target.txt";
    std::ofstream(target) << "target";

    auto result = linglong::utils::concatFile(non_existent_source, target);
    EXPECT_FALSE(result.has_value()) << "Should fail when source file doesn't exist";

    // Test concatenating when source and target are the same file
    fs::path same_file = dest_dir / "same.txt";
    std::ofstream(same_file) << "same content";

    auto result_same = linglong::utils::concatFile(same_file, same_file);
    EXPECT_FALSE(result_same.has_value()) << "Should fail when source and target are the same file";

    // Test concatenating binary content
    fs::path binary_source = dest_dir / "binary_source.txt";
    fs::path binary_target = dest_dir / "binary_target.txt";

    std::string binary_content = "\x01\x02\xFF\xFE\xFD";
    std::ofstream(binary_source, std::ios::binary) << binary_content;
    std::ofstream(binary_target, std::ios::binary) << std::string("\x10\x20", 2);

    auto result_binary = linglong::utils::concatFile(binary_source, binary_target);
    ASSERT_TRUE(result_binary.has_value()) << result_binary.error().message();

    auto read_binary = linglong::utils::readFile(binary_target.string());
    ASSERT_TRUE(read_binary.has_value()) << read_binary.error().message();
    EXPECT_EQ(*read_binary, std::string("\x10\x20", 2) + binary_content);

    // Test concatenating empty source file
    fs::path empty_source = dest_dir / "empty_source.txt";
    fs::path empty_target = dest_dir / "empty_target.txt";

    auto _ = std::ofstream(empty_source); // Create empty file
    std::ofstream(empty_target) << "target content";

    auto result_empty_source = linglong::utils::concatFile(empty_source, empty_target);
    ASSERT_TRUE(result_empty_source.has_value()) << result_empty_source.error().message();

    auto read_empty_source = linglong::utils::readFile(empty_target.string());
    ASSERT_TRUE(read_empty_source.has_value()) << read_empty_source.error().message();
    EXPECT_EQ(*read_empty_source, "target content");
}
