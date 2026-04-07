// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/overlayfs.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace {

class OverlayFSTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        lower_dir = temp_dir.path() / "lower";
        upper_dir = temp_dir.path() / "upper";
        work_dir = temp_dir.path() / "work";
        merged_dir = temp_dir.path() / "rootfs";

        fs::create_directories(lower_dir);
        fs::create_directories(upper_dir);
        fs::create_directories(work_dir);
        fs::create_directories(merged_dir);
    }

    TempDir temp_dir;
    fs::path lower_dir;
    fs::path upper_dir;
    fs::path work_dir;
    fs::path merged_dir;
};

TEST_F(OverlayFSTest, FUSEModeEnforced)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    EXPECT_FALSE(overlay.isMounted()) << "Should not be mounted initially";

    EXPECT_EQ(overlay.getMode(), linglong::utils::OverlayMode::FUSE)
      << "FUSE mode should be enforced";
}

TEST_F(OverlayFSTest, KernelModeEnforced)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::Kernel);

    EXPECT_FALSE(overlay.isMounted()) << "Should not be mounted initially";

    EXPECT_EQ(overlay.getMode(), linglong::utils::OverlayMode::Kernel)
      << "Kernel mode should be enforced";
}

TEST_F(OverlayFSTest, IsMountedInitiallyFalse)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    EXPECT_FALSE(overlay.isMounted()) << "Should not be mounted immediately after construction";
}

TEST_F(OverlayFSTest, GetModeReturnsValidMode)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    auto mode = overlay.getMode();
    EXPECT_TRUE(mode == linglong::utils::OverlayMode::Kernel
                || mode == linglong::utils::OverlayMode::FUSE)
      << "getMode should return a valid OverlayMode";
}

TEST_F(OverlayFSTest, UnmountDoesNotCrashWhenNotMounted)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    EXPECT_FALSE(overlay.isMounted()) << "Should not be mounted initially";

    overlay.unmount();
    EXPECT_FALSE(overlay.isMounted())
      << "Should still not be mounted after unmount on unmounted OverlayFS";
}

TEST_F(OverlayFSTest, GetPathMethods)
{
    linglong::utils::OverlayFS overlay({ lower_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    EXPECT_EQ(overlay.upperDirPath(), upper_dir);
    EXPECT_EQ(overlay.mergedDirPath(), merged_dir);
}

TEST_F(OverlayFSTest, MultipleLowerDirs)
{
    fs::path lower2_dir = temp_dir.path() / "lower2";
    fs::create_directories(lower2_dir);

    linglong::utils::OverlayFS overlay({ lower_dir, lower2_dir },
                                       upper_dir,
                                       work_dir,
                                       merged_dir,
                                       linglong::utils::OverlayMode::FUSE);

    EXPECT_FALSE(overlay.isMounted()) << "Should not be mounted initially";

    auto mode = overlay.getMode();
    EXPECT_EQ(mode, linglong::utils::OverlayMode::FUSE) << "Specified mode should be preserved";
}

} // namespace
