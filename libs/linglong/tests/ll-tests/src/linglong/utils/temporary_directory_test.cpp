// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/finally/finally.h"
#include "linglong/utils/temporary_directory.h"

#include <fstream>
#include <utility>

namespace linglong::utils {
namespace {

TEST(TemporaryDirectory, RequiresExistingParent)
{
    auto parent = TemporaryDirectory::create("linglong-temporary-directory-test-");
    ASSERT_TRUE(parent.has_value()) << parent.error().message();

    const auto missingParent = parent->path() / "missing";
    auto directory = TemporaryDirectory::create("child-", missingParent);

    EXPECT_FALSE(directory.has_value());
    EXPECT_FALSE(std::filesystem::exists(missingParent));
}

TEST(TemporaryDirectory, CreatesUniqueDirectoriesAndCleansRecursively)
{
    std::filesystem::path firstPath;
    std::filesystem::path secondPath;
    {
        auto first = TemporaryDirectory::create("linglong-temporary-directory-test-");
        ASSERT_TRUE(first.has_value()) << first.error().message();
        auto second = TemporaryDirectory::create("linglong-temporary-directory-test-");
        ASSERT_TRUE(second.has_value()) << second.error().message();

        firstPath = first->path();
        secondPath = second->path();
        EXPECT_NE(firstPath, secondPath);
        EXPECT_TRUE(std::filesystem::is_directory(firstPath));
        EXPECT_TRUE(std::filesystem::is_directory(secondPath));

        std::filesystem::create_directories(firstPath / "nested");
        std::ofstream(firstPath / "nested/file") << "temporary";
    }

    EXPECT_FALSE(std::filesystem::exists(firstPath));
    EXPECT_FALSE(std::filesystem::exists(secondPath));
}

TEST(TemporaryDirectory, MoveConstructionTransfersOwnership)
{
    std::filesystem::path path;
    {
        auto created = TemporaryDirectory::create("linglong-temporary-directory-test-");
        ASSERT_TRUE(created.has_value()) << created.error().message();
        path = created->path();

        auto moved = std::move(*created);
        EXPECT_TRUE(created->path().empty());
        EXPECT_EQ(moved.path(), path);
        EXPECT_TRUE(std::filesystem::exists(path));
    }

    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(TemporaryDirectory, MoveAssignmentCleansPreviousDirectory)
{
    auto first = TemporaryDirectory::create("linglong-temporary-directory-test-");
    ASSERT_TRUE(first.has_value()) << first.error().message();
    auto second = TemporaryDirectory::create("linglong-temporary-directory-test-");
    ASSERT_TRUE(second.has_value()) << second.error().message();

    const auto firstPath = first->path();
    const auto secondPath = second->path();
    *second = std::move(*first);

    EXPECT_TRUE(first->path().empty());
    EXPECT_EQ(second->path(), firstPath);
    EXPECT_TRUE(std::filesystem::exists(firstPath));
    EXPECT_FALSE(std::filesystem::exists(secondPath));
}

TEST(TemporaryDirectory, KeepPreservesDirectory)
{
    auto created = TemporaryDirectory::create("linglong-temporary-directory-test-");
    ASSERT_TRUE(created.has_value()) << created.error().message();

    const auto path = created->keep();
    auto cleanup = finally::finally([&path] {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    });

    EXPECT_TRUE(created->path().empty());
    EXPECT_TRUE(std::filesystem::is_directory(path));
}

} // namespace
} // namespace linglong::utils
