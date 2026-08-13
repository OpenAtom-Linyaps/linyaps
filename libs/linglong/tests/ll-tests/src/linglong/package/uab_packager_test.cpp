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

TEST(UABPackagerTest, GenerateExecEntry)
{
    auto entry = detail::generateExecEntry(
      { "/opt/apps/org.deepin.demo/files/bin/demo", "--title", "it's ready" },
      "/opt/apps/org.deepin.demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();

    EXPECT_EQ(*entry,
              "#!/bin/sh\n"
              "set -eu\n"
              ": \"${LINGLONG_UAB_APPROOT:?LINGLONG_UAB_APPROOT is not set}\"\n"
              "cd \"$LINGLONG_UAB_APPROOT\"\n"
              "exec \"$LINGLONG_UAB_APPROOT\"/'bin/demo' '--title' 'it'\\''s ready' \"$@\"\n");
}

TEST(UABPackagerTest, GenerateExecEntryRejectsInvalidCommand)
{
    EXPECT_FALSE(detail::generateExecEntry({}, "/opt/apps/demo/files").has_value());
    EXPECT_FALSE(detail::generateExecEntry({ "./../" }, "/opt/apps/demo/files").has_value());
    EXPECT_FALSE(detail::generateExecEntry({ "../outside" }, "/opt/apps/demo/files").has_value());
}

TEST(UABPackagerTest, GenerateExecEntrySupportsRelativeCommand)
{
    auto entry = detail::generateExecEntry({ "demo" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();
    EXPECT_NE(entry->find("exec \"$LINGLONG_UAB_APPROOT/bin\"/'demo' \"$@\""), std::string::npos);

    entry = detail::generateExecEntry({ "./demo" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();
    EXPECT_NE(entry->find("exec \"$LINGLONG_UAB_APPROOT/bin\"/'demo' \"$@\""), std::string::npos);
}

TEST(UABPackagerTest, GenerateExecEntryKeepsCommandOutsidePrefix)
{
    auto entry = detail::generateExecEntry({ "/usr/bin/demo" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();
    EXPECT_NE(entry->find("exec '/usr/bin/demo' \"$@\""), std::string::npos);

    entry =
      detail::generateExecEntry({ "/opt/apps/demo/files-extra/bin/demo" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();
    EXPECT_NE(entry->find("exec '/opt/apps/demo/files-extra/bin/demo' \"$@\""), std::string::npos);
}

TEST(UABPackagerTest, GenerateExecLoaderForwardsToAppEntry)
{
    EXPECT_EQ(detail::generateExecLoader(),
              "#!/bin/sh\n"
              "set -eu\n"
              ": \"${LINGLONG_UAB_APPROOT:?LINGLONG_UAB_APPROOT is not set}\"\n"
              "exec \"$LINGLONG_UAB_APPROOT/entry.sh\" \"$@\"\n");
}

TEST(UABPackagerTest, EnsureExecEntryPreservesExistingEntryWithoutCommand)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto entryPath = tempDir.path() / "entry.sh";
    constexpr auto existingEntry = "#!/bin/sh\nexec /usr/bin/demo \"$@\"\n";
    std::ofstream{ entryPath } << existingEntry;
    std::filesystem::permissions(entryPath,
                                 std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add);

    auto result = detail::ensureExecEntry(entryPath, std::nullopt, "/opt/apps/demo/files");
    ASSERT_TRUE(result.has_value()) << result.error().message();

    std::ifstream entry{ entryPath };
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>{ entry }, {}), existingEntry);

    const auto permissions = std::filesystem::status(entryPath).permissions();
    EXPECT_NE(permissions & std::filesystem::perms::owner_exec, std::filesystem::perms::none);
    EXPECT_EQ(permissions & std::filesystem::perms::group_exec, std::filesystem::perms::none);
    EXPECT_EQ(permissions & std::filesystem::perms::others_exec, std::filesystem::perms::none);
}

TEST(UABPackagerTest, EnsureExecEntryRejectsNonExecutableExistingEntry)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto entryPath = tempDir.path() / "entry.sh";
    std::ofstream{ entryPath } << "#!/bin/sh\nexec /usr/bin/demo \"$@\"\n";
    std::filesystem::permissions(entryPath,
                                 std::filesystem::perms::owner_read
                                   | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);

    const auto permissions = std::filesystem::status(entryPath).permissions();
    auto result = detail::ensureExecEntry(entryPath, std::nullopt, "/opt/apps/demo/files");

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("is not executable"), std::string::npos);
    EXPECT_EQ(std::filesystem::status(entryPath).permissions(), permissions);
}

TEST(UABPackagerTest, EnsureExecEntryRejectsExistingDirectory)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto entryPath = tempDir.path() / "entry.sh";
    ASSERT_TRUE(std::filesystem::create_directory(entryPath));

    auto result = detail::ensureExecEntry(entryPath, std::nullopt, "/opt/apps/demo/files");

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("is not a regular file"), std::string::npos);
}

TEST(UABPackagerTest, EnsureExecEntryGeneratesMissingEntry)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto entryPath = tempDir.path() / "entry.sh";
    auto result = detail::ensureExecEntry(entryPath,
                                          std::vector<std::string>{
                                            "/opt/apps/demo/files/bin/demo",
                                          },
                                          "/opt/apps/demo/files");
    ASSERT_TRUE(result.has_value()) << result.error().message();

    std::ifstream entry{ entryPath };
    const auto content = std::string(std::istreambuf_iterator<char>{ entry }, {});
    EXPECT_NE(content.find("exec \"$LINGLONG_UAB_APPROOT\"/'bin/demo' \"$@\""), std::string::npos);

    const auto permissions = std::filesystem::status(entryPath).permissions();
    EXPECT_NE(permissions & std::filesystem::perms::owner_exec, std::filesystem::perms::none);
}

} // namespace
} // namespace linglong::package
