// SPDX-FileCopyrightText: 2025-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "nvidia_driver_detector.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace linglong::driver::detect;
using ::testing::_;
using ::testing::Return;

// A mock class that only mocks checkPackageInstalled, as getDriverVersion is no longer virtual
class TestableNVIDIADriverDetector : public NVIDIADriverDetector
{
public:
    explicit TestableNVIDIADriverDetector(std::string versionFilePath)
        : NVIDIADriverDetector(std::move(versionFilePath))
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

class NvidiaDriverDetectorTest : public ::testing::Test
{
protected:
    // Helper to create a mock version file
    void createMockVersionFile(std::filesystem::path path, const std::string &content)
    {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open()) << "Failed to create mock version file: " << path;
        file << content;
        file.close();
    }
};

TEST_F(NvidiaDriverDetectorTest, CompareVersionsUsesSemanticOrdering)
{
    TestableNVIDIADriverDetector detector("unused-version-file");

    EXPECT_TRUE(detector.isVersionNewer("510.100.0", "510.99.0"));
    EXPECT_FALSE(detector.isVersionNewer("510.99.0", "510.100.0"));
}

TEST_F(NvidiaDriverDetectorTest, Detect_Success_PackageNotInstalled)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "nvidia_test_version_file.txt";
    createMockVersionFile(versionFilePath, "510.85.02"); // Simplified version format now
    TestableNVIDIADriverDetector detector(versionFilePath.string());
    const std::string expectedVersion = "510-85-02"; // With dots replaced by dashes
    const std::string expectedPackageName =
      std::string(detector.getDriverIdentify()) + "." + expectedVersion;

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPackageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = expectedPackageName,
                                        .packageVersion = expectedVersion,
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(false));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->packageName, expectedPackageName);
    EXPECT_EQ(result->packageVersion, expectedVersion);
}

TEST_F(NvidiaDriverDetectorTest, Detect_Success_PackageInstalled)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "nvidia_test_version_file.txt";
    createMockVersionFile(versionFilePath, "510.85.02"); // Simplified version format now
    TestableNVIDIADriverDetector detector(versionFilePath.string());
    const std::string expectedVersion = "510-85-02"; // With dots replaced by dashes
    const std::string expectedPackageName =
      std::string(detector.getDriverIdentify()) + "." + expectedVersion;

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPackageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = expectedPackageName,
                                        .packageVersion = expectedVersion,
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(true));
    EXPECT_CALL(detector, checkPackageUpgradable(expectedPackageName)).WillOnce(Return(false));

    auto result = detector.detect();

    ASSERT_FALSE(result.has_value());
}

TEST_F(NvidiaDriverDetectorTest, Detect_Success_PackageUpgradable)
{
    TempDir temp_dir;
    auto versionFilePath = temp_dir.path() / "nvidia_test_version_file.txt";
    createMockVersionFile(versionFilePath, "510.85.02");
    TestableNVIDIADriverDetector detector(versionFilePath.string());
    const std::string expectedVersion = "510-85-02";
    const std::string upgradableExpectedVersion = "510-85-03";
    const std::string expectedPackageName =
      std::string(detector.getDriverIdentify()) + "." + expectedVersion;

    EXPECT_CALL(detector, getPackageInfoFromRemoteRepo(expectedPackageName))
      .WillOnce(Return(std::make_pair(true,
                                      GraphicsDriverInfo{
                                        .identify = detector.getDriverIdentify(),
                                        .packageName = expectedPackageName,
                                        .packageVersion = upgradableExpectedVersion,
                                      })));

    EXPECT_CALL(detector, checkPackageInstalled(expectedPackageName)).WillOnce(Return(true));
    EXPECT_CALL(detector, checkPackageUpgradable(expectedPackageName)).WillOnce(Return(true));

    auto result = detector.detect();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->identify, detector.getDriverIdentify());
    EXPECT_EQ(result->packageName, expectedPackageName);
    EXPECT_EQ(result->packageVersion, upgradableExpectedVersion);
}

namespace {

class ScopedEnv
{
public:
    ScopedEnv(const std::string &name, const std::string &value)
        : name_(name)
    {
        const char *old = ::getenv(name_.c_str());
        if (old != nullptr) {
            old_ = old;
            existed_ = true;
        }
        ::setenv(name_.c_str(), value.c_str(), 1);
    }

    ~ScopedEnv()
    {
        if (existed_) {
            ::setenv(name_.c_str(), old_.c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    std::string old_;
    bool existed_{ false };
};

class FakeCli
{
public:
    explicit FakeCli(const TempDir &dir)
        : path_(dir.path() / "ll-cli")
    {
        std::ofstream out(path_);
        out << "#!/bin/sh\n"
               "if [ \"$1\" = \"--json\" ] && [ \"$2\" = \"search\" ]; then\n"
               "  printf '%s' \"$LL_TEST_SEARCH_JSON\"\n"
               "  exit 0\n"
               "fi\n"
               "if [ \"$1\" = \"--json\" ] && [ \"$2\" = \"list\" ]; then\n"
               "  printf '%s' \"$LL_TEST_LIST_JSON\"\n"
               "  exit 0\n"
               "fi\n"
               "exit 1\n";
        out.close();
        std::filesystem::permissions(path_,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }

    const std::filesystem::path &path() const { return path_; }

private:
    std::filesystem::path path_;
};

std::filesystem::path makeVersionFile(const TempDir &dir, const std::string &version)
{
    auto path = dir.path() / "nvidia-version";
    std::ofstream(path) << version;
    return path;
}

constexpr const char *kPackage = "org.deepin.driver.display.nvidia.1-0-0";

} // namespace

TEST_F(NvidiaDriverDetectorTest, DetectUsesHighestRemoteVersionWhenNotInstalled)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search(
      "LL_TEST_SEARCH_JSON",
      R"({"stable":[{"version":"0.9.0"},{"version":"1.0.1"}],"beta":[{"version":"1.0.0"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON", "[]");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->identify, NVIDIADriverDetector::kNvidiaPackageIdentify);
    EXPECT_EQ(result->packageName, kPackage);
    EXPECT_EQ(result->packageVersion, "1.0.1");
    EXPECT_EQ(result->repoName, "stable");
}

TEST_F(NvidiaDriverDetectorTest, DetectReportsAlreadyInstalledAndUpToDate)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":[{"version":"1.0.1"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON",
                   R"([{"id":"org.deepin.driver.display.nvidia.1-0-0","version":"1.0.1"}])");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("already installed and up-to-date"));
}

TEST_F(NvidiaDriverDetectorTest, DetectReturnsRemoteInfoWhenInstalledVersionCanUpgrade)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":[{"version":"1.0.1"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON",
                   R"([{"id":"org.deepin.driver.display.nvidia.1-0-0","version":"1.0.0"}])");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->packageVersion, "1.0.1");
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWithoutVersionFile)
{
    TempDir dir;
    FakeCli cli(dir);

    NVIDIADriverDetector detector((dir.path() / "missing").string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("Failed to get NVIDIA driver version"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenSearchCommandFails)
{
    TempDir dir;
    auto script = dir.path() / "ll-cli";
    std::ofstream(script) << "#!/bin/sh\nexit 3\n";
    std::filesystem::permissions(script,
                                 std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), script.string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("Search command failed"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenSearchJsonIsMalformed)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", "not-json");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("Failed to parse search result JSON"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenInstalledJsonIsMalformed)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":[{"version":"1.0.1"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON", "not-json");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("Failed to parse installed package JSON"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenInstalledJsonIsNotArray)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":[{"version":"1.0.1"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON", R"({"id":"x"})");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("expected an array"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenInstalledEntryHasNoVersion)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":[{"version":"1.0.1"}]})");
    ScopedEnv list("LL_TEST_LIST_JSON", R"([{"id":"org.deepin.driver.display.nvidia.1-0-0"}])");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("version field is missing"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenInstalledButRemoteSearchIsEmpty)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", R"({"stable":"not-array"})");
    ScopedEnv list("LL_TEST_LIST_JSON",
                   R"([{"id":"org.deepin.driver.display.nvidia.1-0-0","version":"1.0.1"}])");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("Cannot find NVIDIA driver package in remote repo"));
}

TEST_F(NvidiaDriverDetectorTest, DetectFailsWhenNotInstalledAndRemoteSearchIsEmpty)
{
    TempDir dir;
    FakeCli cli(dir);
    ScopedEnv search("LL_TEST_SEARCH_JSON", "{}");
    ScopedEnv list("LL_TEST_LIST_JSON", "[]");

    NVIDIADriverDetector detector(makeVersionFile(dir, "1.0.0").string(), cli.path().string());
    auto result = detector.detect();

    EXPECT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                ::testing::HasSubstr("NVIDIA driver package not found in remote repo"));
}
