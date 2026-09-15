// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "container_tool_detector.h"
#include "linglong/utils/error/error.h"

#include <set>
#include <string>
#include <vector>

namespace {

using namespace linglong::ctk::detect;

constexpr auto kDeviceId = "nvidia";
constexpr auto kToolPackage = "nvidia-container-toolkit-base";
constexpr auto kSecondPackage = "libnvidia-container-tools";

detail::PackageStatusQuery queryReturning(const std::string &output)
{
    return
      [output](const std::vector<std::string> &) -> linglong::utils::error::Result<std::string> {
          return output;
      };
}

detail::PackageStatusQuery failingQuery()
{
    return [](const std::vector<std::string> &) -> linglong::utils::error::Result<std::string> {
        return tl::unexpected(
          linglong::utils::error::Error::Err(__FILE__, __LINE__, "test", "dpkg-query failed"));
    };
}

ContainerToolSpec makeTool(std::vector<std::string> packages = { kToolPackage })
{
    return { .deviceId = kDeviceId, .packages = std::move(packages) };
}

std::set<std::string> detected(const std::string &deviceId)
{
    return { deviceId };
}

} // namespace

TEST(ContainerToolDetectorTest, IsPackageInstalledMatchesExactPackageLine)
{
    EXPECT_TRUE(detail::isPackageInstalled(kToolPackage,
                                           "ii  nvidia-container-toolkit-base\n"
                                           "ii  libnvidia-container-tools\n"));
}

TEST(ContainerToolDetectorTest, IsPackageInstalledRejectsOtherStatuses)
{
    EXPECT_FALSE(detail::isPackageInstalled(kToolPackage, "un  nvidia-container-toolkit-base\n"));
}

TEST(ContainerToolDetectorTest, IsPackageInstalledAcceptsHeldInstalledPackage)
{
    EXPECT_TRUE(detail::isPackageInstalled(kToolPackage, "hi  nvidia-container-toolkit-base\n"));
}

TEST(ContainerToolDetectorTest, IsPackageInstalledIgnoresArchitectureQualifier)
{
    EXPECT_TRUE(
      detail::isPackageInstalled(kToolPackage, "ii  nvidia-container-toolkit-base:amd64\n"));
}

TEST(ContainerToolDetectorTest, IsPackageInstalledRejectsPrefixMatches)
{
    EXPECT_FALSE(
      detail::isPackageInstalled(kToolPackage, "ii  nvidia-container-toolkit-base-extra\n"));
}

TEST(ContainerToolDetectorTest, MissingPackageIsReportedWhenDeviceIsDetected)
{
    const std::vector<ContainerToolSpec> tools{ makeTool() };
    auto missing =
      detail::findMissingContainerTools(tools, detected(kDeviceId), queryReturning(""));

    ASSERT_EQ(missing.size(), 1);
    EXPECT_EQ(missing.front().deviceId, kDeviceId);
    ASSERT_EQ(missing.front().packages.size(), 1);
    EXPECT_EQ(missing.front().packages.front(), kToolPackage);
}

TEST(ContainerToolDetectorTest, InstalledPackageIsNotReportedMissing)
{
    const std::vector<ContainerToolSpec> tools{ makeTool() };
    auto missing =
      detail::findMissingContainerTools(tools,
                                        detected(kDeviceId),
                                        queryReturning("ii  nvidia-container-toolkit-base\n"));

    EXPECT_TRUE(missing.empty());
}

TEST(ContainerToolDetectorTest, ToolIsSkippedWhenDeviceIsNotDetected)
{
    const std::vector<ContainerToolSpec> tools{ makeTool() };
    auto missing =
      detail::findMissingContainerTools(tools, std::set<std::string>{}, queryReturning(""));

    EXPECT_TRUE(missing.empty());
}

TEST(ContainerToolDetectorTest, QueryFailureIsTreatedAsUnknownAndSkipsAllTools)
{
    const std::vector<ContainerToolSpec> tools{ makeTool() };
    auto missing = detail::findMissingContainerTools(tools, detected(kDeviceId), failingQuery());

    EXPECT_TRUE(missing.empty());
}

TEST(ContainerToolDetectorTest, OnlyMissingPackagesAreReturned)
{
    const std::vector<ContainerToolSpec> tools{ makeTool({ kToolPackage, kSecondPackage }) };
    auto missing =
      detail::findMissingContainerTools(tools,
                                        detected(kDeviceId),
                                        queryReturning("ii  nvidia-container-toolkit-base\n"));

    ASSERT_EQ(missing.size(), 1);
    EXPECT_EQ(missing.front().deviceId, kDeviceId);
    ASSERT_EQ(missing.front().packages.size(), 1);
    EXPECT_EQ(missing.front().packages.front(), kSecondPackage);
}
