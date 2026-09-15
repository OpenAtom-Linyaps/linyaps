// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "nvidia_device_detector.h"

#include <filesystem>

namespace {

using namespace linglong::ctk::detect;

class NVIDIADeviceDetectorTest : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(NVIDIADeviceDetectorTest, DetectsLoadedModule)
{
    std::filesystem::create_directories(tempDir.path() / "nvidia");
    NVIDIADeviceDetector detector("nvidia", tempDir.path());

    auto result = detector.detect();

    ASSERT_TRUE(result);
    ASSERT_TRUE(*result);
    EXPECT_EQ(**result, "nvidia");
}

TEST_F(NVIDIADeviceDetectorTest, ReturnsNulloptWhenModuleIsAbsent)
{
    NVIDIADeviceDetector detector("nvidia", tempDir.path());

    auto result = detector.detect();

    ASSERT_TRUE(result);
    EXPECT_FALSE(*result);
}

TEST_F(NVIDIADeviceDetectorTest, DoesNotConfuseDirectoryPrefixes)
{
    std::filesystem::create_directories(tempDir.path() / "nvidia_modeset");
    NVIDIADeviceDetector detector("nvidia", tempDir.path());

    auto result = detector.detect();

    ASSERT_TRUE(result);
    EXPECT_FALSE(*result);
}

} // namespace
