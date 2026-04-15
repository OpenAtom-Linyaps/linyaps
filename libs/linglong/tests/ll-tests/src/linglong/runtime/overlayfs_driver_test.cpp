// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/runtime/overlayfs_driver.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace {

class OverlayFSDriverTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        bundle_dir = temp_dir.path() / "bundle";
        lower_dir = temp_dir.path() / "lower";
        merged_dir = bundle_dir / "rootfs";

        fs::create_directories(bundle_dir);
        fs::create_directories(lower_dir);
        fs::create_directories(merged_dir);
    }

    TempDir temp_dir;
    fs::path bundle_dir;
    fs::path lower_dir;
    fs::path merged_dir;
};

TEST(OverlayFSDriverStatic, DetectKernelOverlaySupport)
{
    auto support = linglong::runtime::OverlayFSDriver::detectKernelOverlaySupport();

    EXPECT_TRUE(support.canUseInUserNS == (support.kernelAvailable && support.kernelVersionOK))
      << "canUseInUserNS should reflect kernel availability and version check";

    std::cout << "Kernel overlay detection results:\n"
              << "  kernel available: " << (support.kernelAvailable ? "yes" : "no") << "\n"
              << "  kernel version >= 5.11: " << (support.kernelVersionOK ? "yes" : "no") << "\n"
              << "  can use in user NS: " << (support.canUseInUserNS ? "yes" : "no") << std::endl;
}

TEST(OverlayFSDriverStatic, CanUseKernelOverlay)
{
    bool canUse = linglong::runtime::OverlayFSDriver::canUseKernelOverlay();

    auto support = linglong::runtime::OverlayFSDriver::detectKernelOverlaySupport();
    EXPECT_EQ(canUse, support.canUseInUserNS)
      << "canUseKernelOverlay should match canUseInUserNS from KernelOverlaySupport";

    std::cout << "canUseKernelOverlay: " << (canUse ? "yes" : "no") << std::endl;
}

TEST_F(OverlayFSDriverTest, AutoModeSelection)
{
    auto driver = linglong::runtime::OverlayFSDriver::create(linglong::utils::OverlayMode::Auto);
    auto mode = driver->mode();
    EXPECT_TRUE(mode == linglong::utils::OverlayMode::Kernel
                || mode == linglong::utils::OverlayMode::FUSE)
      << "Auto mode should select either Kernel or FUSE mode";

    auto support = linglong::runtime::OverlayFSDriver::detectKernelOverlaySupport();

    if (support.canUseInUserNS) {
        EXPECT_EQ(mode, linglong::utils::OverlayMode::Kernel)
          << "Auto mode should prefer Kernel when available";
    } else {
        EXPECT_EQ(mode, linglong::utils::OverlayMode::FUSE) << "Auto mode should fallback to FUSE";
    }

    if (support.kernelAvailable) {
        std::cout << "Auto mode selected: "
                  << (mode == linglong::utils::OverlayMode::Kernel ? "Kernel" : "FUSE")
                  << " (kernel available)" << std::endl;
    } else {
        std::cout << "Auto mode selected: FUSE (kernel not available)" << std::endl;
    }
}

TEST_F(OverlayFSDriverTest, KernelDriverNonPersistentUsesUpperdirAsLowerLayer)
{
    auto overlay_internal = temp_dir.path() / "overlay-internal";
    auto driver = linglong::runtime::OverlayFSDriver::create(linglong::utils::OverlayMode::Kernel);

    auto overlay = driver->createOverlayFS({ lower_dir }, overlay_internal, bundle_dir, false);

    ASSERT_TRUE(overlay);
    EXPECT_EQ((*overlay)->getMode(), linglong::utils::OverlayMode::Kernel);
    ASSERT_TRUE((*overlay)->upperDirPath().has_value());
    ASSERT_TRUE((*overlay)->workDirPath().has_value());
    EXPECT_EQ((*overlay)->upperDirPath(), bundle_dir / "overlay" / "upperdir");
    EXPECT_EQ((*overlay)->workDirPath(), bundle_dir / "overlay" / "workdir");
    EXPECT_EQ((*overlay)->mergedDirPath(), merged_dir);

    std::vector<fs::path> expectedLowerdirs{ overlay_internal / "upperdir", lower_dir };
    EXPECT_EQ((*overlay)->lowerDirPaths(), expectedLowerdirs);
}

TEST_F(OverlayFSDriverTest, KernelDriverPersistentCreatesUpperdirAndWorkdir)
{
    auto overlay_internal = temp_dir.path() / "overlay-internal";
    auto driver = linglong::runtime::OverlayFSDriver::create(linglong::utils::OverlayMode::Kernel);

    auto overlay = driver->createOverlayFS({ lower_dir }, overlay_internal, bundle_dir, true);

    ASSERT_TRUE(overlay);
    ASSERT_TRUE((*overlay)->upperDirPath().has_value());
    ASSERT_TRUE((*overlay)->workDirPath().has_value());
    EXPECT_EQ((*overlay)->mergedDirPath(), merged_dir);
    EXPECT_TRUE(fs::exists(*(*overlay)->upperDirPath()));
    EXPECT_TRUE(fs::exists(*(*overlay)->workDirPath()));
}

} // namespace
