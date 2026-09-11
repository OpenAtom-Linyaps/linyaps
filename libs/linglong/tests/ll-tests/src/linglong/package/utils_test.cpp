/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/utils.h"
#include "linglong/utils/error/error.h"

#include <string>

namespace linglong::package::test {

TEST(ValidateExecutableNameTest, AcceptsNormalName)
{
    auto result = validateExecutableName("ls");
    EXPECT_TRUE(result.has_value());
}

TEST(ValidateExecutableNameTest, AcceptsDotsInsideName)
{
    auto result = validateExecutableName("my.tool");
    EXPECT_TRUE(result.has_value());
}

TEST(ValidateExecutableNameTest, AcceptsMaxLength255)
{
    std::string name(255, 'a');
    auto result = validateExecutableName(name);
    EXPECT_TRUE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsEmpty)
{
    auto result = validateExecutableName("");
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsDot)
{
    auto result = validateExecutableName(".");
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsDoubleDot)
{
    auto result = validateExecutableName("..");
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsSlash)
{
    auto result = validateExecutableName("a/b");
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsLeadingSlash)
{
    auto result = validateExecutableName("/ls");
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsNul)
{
    std::string name;
    name.push_back('a');
    name.push_back('\0');
    name.push_back('b');
    auto result = validateExecutableName(name);
    EXPECT_FALSE(result.has_value());
}

TEST(ValidateExecutableNameTest, RejectsTooLong)
{
    std::string name(256, 'a');
    auto result = validateExecutableName(name);
    EXPECT_FALSE(result.has_value());
}

} // namespace linglong::package::test
