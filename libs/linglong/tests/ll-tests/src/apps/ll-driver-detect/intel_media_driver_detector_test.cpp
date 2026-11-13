// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "intel_media_driver_detector.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace linglong::driver::detect;
using ::testing::_;
using ::testing::Return;

// Exposes the protected compareVersions helper and mocks every path that would
// otherwise shell out to ll-cli, so the whole detection flow can be exercised
// without a real repository or an Intel GPU.
class TestableIntelMediaDriverDetector : public IntelMediaDriverDetector
{
public:
    explicit TestableIntelMediaDriverDetector(
      std::vector<std::filesystem::path> intelGpuModulePaths)
        : IntelMediaDriverDetector(std::move(intelGpuModulePaths))
    {
    }

    bool isVersionNewer(std::string_view candidate, std::string_view current) const noexcept
    {
        return compareVersions(candidate, current);
    }

    MOCK_METHOD((linglong::utils::error::Result<std::pair<bool, GraphicsDriverInfo>>),
                getPackageInfoFromRemoteRepo,
                (const std::string &packageName),
                (override));

    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageInstalled,
                (const std::string &),
                (override));
    MOCK_METHOD(linglong::utils::error::Result<bool>,
                checkPackageUpgradable,
                (const std::string &),
                (override));
};

class IntelMediaDriverDetectorTest : public ::testing::Test
{
protected:
    // The kernel module directory is the only hardware dependent input of the
    // detector, so materialising it in a temp dir keeps these tests host agnostic.
    static std::filesystem::path createGpuModulePath(const TempDir &tempDir)
    {
        auto modulePath = tempDir.path() / "i915";
        std::filesystem::create_directories(modulePath);
        return modulePath;
    }
};

TEST_F(IntelMediaDriverDetectorTest, CompareVersionsUsesSemanticOrdering)
{
    TestableIntelMediaDriverDetector detector{ std::vector<std::filesystem::path>{} };

    // "1.0.10" is newer than "1.0.9" semantically, which a plain string compare
    // would get wrong.
    EXPECT_TRUE(detector.isVersionNewer("1.0.10", "1.0.9"));
    EXPECT_FALSE(detector.isVersionNewer("1.0.9", "1.0.10"));
}

TEST_F(IntelMediaDriverDetectorTest, Detect_NoIntelGpu)
{
    TempDir tempDir;
    // None of the injected module paths exist, so no Intel GPU is present. The
    // detector must report an error without ever querying the repository.
    TestableIntelMediaDriverDetector detector({ tempDir.path() / "does-not-exist" });

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(_)).Times(0);
    EXPECT_CALL(detector, checkPackageInstalled(_)).Times(0);

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
}

TEST_F(IntelMediaDriverDetectorTest, Detect_Success_PackageNotInstalled)
{
    TempDir tempDir;
    TestableIntelMediaDriverDetector detector({ createGpuModulePath(tempDir) });

    const std::string expectedPackageName = detector.getDriverIdentify();
    const std::string expectedVersion = "1.0.0";
    const std::string expectedRepo = "stable";

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPackageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = expectedPackageName,
                                        .packageVersion = expectedVersion,
                                        .repoName = expectedRepo,
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(false));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->packageName, expectedPackageName);
    EXPECT_EQ(result->packageVersion, expectedVersion);
    EXPECT_EQ(result->repoName, expectedRepo);
}

TEST_F(IntelMediaDriverDetectorTest, Detect_Success_PackageUpgradable)
{
    TempDir tempDir;
    TestableIntelMediaDriverDetector detector({ createGpuModulePath(tempDir) });

    const std::string packageName = detector.getDriverIdentify();
    const std::string newerVersion = "1.0.1";

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(packageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = packageName,
                                        .packageVersion = newerVersion,
                                        .repoName = "stable",
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(packageName)).WillOnce(Return(true));
    EXPECT_CALL(detector, checkPackageUpgradable(packageName)).WillOnce(Return(true));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->packageName, packageName);
    EXPECT_EQ(result->packageVersion, newerVersion);
}

TEST_F(IntelMediaDriverDetectorTest, Detect_Success_PackageInstalledAndUpToDate)
{
    TempDir tempDir;
    TestableIntelMediaDriverDetector detector({ createGpuModulePath(tempDir) });

    const std::string packageName = detector.getDriverIdentify();

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(packageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = packageName,
                                        .packageVersion = "1.0.0",
                                        .repoName = "stable",
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(packageName)).WillOnce(Return(true));
    EXPECT_CALL(detector, checkPackageUpgradable(packageName)).WillOnce(Return(false));

    auto result = detector.detect();

    // Nothing to offer when the installed package is already the newest one.
    ASSERT_FALSE(result.has_value());
}

TEST_F(IntelMediaDriverDetectorTest, Detect_GpuPresentButPackageMissingInRepo)
{
    TempDir tempDir;
    TestableIntelMediaDriverDetector detector({ createGpuModulePath(tempDir) });

    const std::string packageName = detector.getDriverIdentify();

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(packageName))
      .WillOnce(Return(std::make_pair(false,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = packageName,
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(packageName)).WillOnce(Return(false));

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
}

TEST_F(IntelMediaDriverDetectorTest, Detect_RemoteRepoQueryFailure)
{
    LINGLONG_TRACE("Testing remote repo query failure");

    TempDir tempDir;
    TestableIntelMediaDriverDetector detector({ createGpuModulePath(tempDir) });

    const std::string packageName = detector.getDriverIdentify();

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(packageName))
      .WillOnce(Return(LINGLONG_ERR("ll-cli search failed")));

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
}
