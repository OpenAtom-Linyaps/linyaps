// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/builder/source_cache.h"

#include <filesystem>

namespace linglong::builder {

TEST(SourceCacheLayoutTest, initializesRootAndTemporary)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());

    const auto cacheRoot = directory.path() / "sources/v1";
    EXPECT_EQ(layout.root(), cacheRoot);
    EXPECT_EQ(layout.temporary(), cacheRoot / "tmp");
}

TEST(SourceCacheLayoutTest, objectWithoutHashSeparatorUsesKindPath)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.object("git", "abcdef0123456789"), cacheRoot / "git" / "abcdef0123456789");
}

TEST(SourceCacheLayoutTest, objectWithHashSeparatorSplitsAlgorithmAndDigest)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.object("git", "sha256:0123abcd"), cacheRoot / "git" / "sha256" / "0123abcd");
}

TEST(SourceCacheLayoutTest, objectWithMultipleSeparatorsNormalizesAlgorithmDashes)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.object("archive", "sha256:base64:abc"),
              cacheRoot / "archive" / "sha256-base64" / "abc");
}

TEST(SourceCacheLayoutTest, gitObjectLowercasesCommitWithoutSubmodules)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.gitObject("AbC123DeF", false),
              cacheRoot / "git/commit" / "abc123def-no-submodules");
}

TEST(SourceCacheLayoutTest, gitObjectLowercasesCommitWithSubmodulesSuffix)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.gitObject("DEADBEEF", true), cacheRoot / "git/commit" / "deadbeef-submodules");
}

TEST(SourceCacheLayoutTest, lockFileKeepsEntryRelativePathUnderLocks)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    const auto entry = layout.object("git", "sha256:abc");
    EXPECT_EQ(layout.lockFile(entry), cacheRoot / "locks" / "git" / "sha256" / "abc.lock");
}

TEST(SourceCacheLayoutTest, lockFileAppendsLockSuffixToNestedEntry)
{
    TempDir directory;
    const SourceCacheLayout layout(directory.path());
    const auto cacheRoot = directory.path() / "sources/v1";

    EXPECT_EQ(layout.lockFile(cacheRoot / "git/commit" / "abc"),
              cacheRoot / "locks" / "git/commit" / "abc.lock");
}

TEST(SourceCacheLayoutTest, ensureCreatesTemporaryDirectory)
{
    TempDir directory;
    SourceCacheLayout layout(directory.path());

    const auto result = layout.ensure();
    ASSERT_TRUE(result.has_value()) << "ensure failed: " << result.error().message();
    EXPECT_TRUE(layout.root().is_absolute());
    EXPECT_TRUE(layout.temporary().is_absolute());
    EXPECT_TRUE(std::filesystem::is_directory(layout.temporary()));
}

} // namespace linglong::builder
