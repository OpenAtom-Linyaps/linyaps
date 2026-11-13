/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/hooks.h"
#include "linglong/utils/overlayfs.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

namespace fs = std::filesystem;

TEST(UtilsOverlayEdge, FuseMountFailsWhenHelperIsMissing)
{
    TempDir tempDir;
    const auto lower = tempDir.path() / "lower";
    const auto merged = tempDir.path() / "merged";
    fs::create_directories(lower);
    fs::create_directories(merged);

    linglong::utils::OverlayFS overlay({ lower },
                                       std::nullopt,
                                       std::nullopt,
                                       merged,
                                       linglong::utils::OverlayMode::FUSE,
                                       (tempDir.path() / "missing-fuse-overlayfs").string());
    EXPECT_FALSE(overlay.mount());
    EXPECT_FALSE(overlay.isMounted());
}

TEST(UtilsHookEdge, ParseInstallHooksLoadsCommandsFromConfigDir)
{
    TempDir tempDir;
    const auto hooksDir = tempDir.path() / "config.d";
    fs::create_directories(hooksDir);

    const auto hookFile = hooksDir / "ll-tests-install-hooks.conf";
    {
        std::ofstream out(hookFile);
        ASSERT_TRUE(out.is_open()) << "cannot write install hook file " << hookFile;
        out << "ll-pre-install=\"echo pre-install\"\n"
               "ll-post-install='echo post-install'\n"
               "ll-post-uninstall=echo post-uninstall\n"
               "# unrelated line\n";
    }

    linglong::utils::InstallHookManager manager(hooksDir);
    auto parsed = manager.parseInstallHooks();
    ASSERT_TRUE(parsed) << parsed.error().message();
}

TEST(UtilsHookEdge, ParseInstallHooksRejectsMalformedHookFile)
{
    TempDir tempDir;
    const auto hooksDir = tempDir.path() / "config.d";
    fs::create_directories(hooksDir);

    const auto hookFile = hooksDir / "ll-tests-malformed-hooks.conf";
    {
        std::ofstream out(hookFile);
        ASSERT_TRUE(out.is_open()) << "cannot write install hook file " << hookFile;
        out << "ll-pre-install=\"unterminated\n";
    }

    linglong::utils::InstallHookManager manager(hooksDir);
    auto parsed = manager.parseInstallHooks();
    EXPECT_FALSE(parsed);
    EXPECT_THAT(parsed.error().message(), ::testing::HasSubstr("Invalid install hook command"));
}

} // namespace
