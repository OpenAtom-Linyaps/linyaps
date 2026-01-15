// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/xdg.h"
#include "linglong/utils/env.h"

#include <filesystem>

class XDGTest : public ::testing::Test
{
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(XDGTest, GetXDGRuntimeDir)
{
    auto checkRuntimeDir = [](const std::filesystem::path &runtimeDir) {
        if (std::filesystem::exists(runtimeDir)) {
            // 1. if dir exists, it should be writable for the owner.
            auto perms = std::filesystem::status(runtimeDir).permissions();
            ASSERT_NE((perms & std::filesystem::perms::owner_write), std::filesystem::perms::none)
              << "runtimeDir is not writable for owner: " << runtimeDir;
        } else {
            // 2. if dir does not exist, its parent dir should be writable by all users.
            auto parentDir = runtimeDir.parent_path();
            ASSERT_TRUE(std::filesystem::exists(parentDir))
              << "parent of runtimeDir does not exist: " << parentDir;
            auto perms = std::filesystem::status(parentDir).permissions();
            ASSERT_NE((perms & std::filesystem::perms::others_write), std::filesystem::perms::none)
              << "parent of runtimeDir is not writable by others: " << parentDir;
        }
    };

    {
        auto runtimeDir = linglong::common::xdg::getXDGRuntimeDir();
        checkRuntimeDir(runtimeDir);
    }

    {
        linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", "/tmp");
        auto runtimeDir = linglong::common::xdg::getXDGRuntimeDir();
        ASSERT_EQ(runtimeDir, "/tmp");
    }

    {
        linglong::utils::EnvironmentVariableGuard env("XDG_RUNTIME_DIR", "");
        auto runtimeDir = linglong::common::xdg::getXDGRuntimeDir();
        checkRuntimeDir(runtimeDir);
    }
}
