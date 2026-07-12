// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <exception>
#include <filesystem>

TEST(CrunNewTest, ReturnsValidObjectWhenBinaryExists)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    auto crun = ocppi::cli::crun::Crun::New(tempDir.path());
    ASSERT_TRUE(crun.has_value());
    EXPECT_EQ((*crun)->bin(), tempDir.path());
}

TEST(CrunNewTest, ReturnsErrorWhenBinaryDoesNotExist)
{
    auto nonexistent = std::filesystem::temp_directory_path() / "linglong-crun-does-not-exist";
    ASSERT_FALSE(std::filesystem::exists(nonexistent));

    auto crun = ocppi::cli::crun::Crun::New(nonexistent);
    ASSERT_FALSE(crun.has_value());
    EXPECT_THROW(std::rethrow_exception(crun.error()), std::exception);
}
