// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "container_tool_config.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

void writeFile(const std::filesystem::path &path, const std::string &content)
{
    std::ofstream file(path);
    file << content;
}

} // namespace

class ContainerToolConfigTest : public ::testing::Test
{
protected:
    TempDir tempDir;
};

TEST_F(ContainerToolConfigTest, NoConfigFilesMeansNoTools)
{
    using namespace linglong::ctk::detect;

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    EXPECT_TRUE(manager.tools().empty());
    EXPECT_FALSE(manager.isNeverRemind("nvidia"));
}

TEST_F(ContainerToolConfigTest, LoadsSystemTools)
{
    using namespace linglong::ctk::detect;

    writeFile(tempDir.path() / "system.json",
              R"({
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base"
      ]
    }
  ]
})");

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    ASSERT_EQ(manager.tools().size(), 1);
    EXPECT_EQ(manager.tools().front().deviceId, "nvidia");
    ASSERT_EQ(manager.tools().front().packages.size(), 1);
    EXPECT_EQ(manager.tools().front().packages.front(), "nvidia-container-toolkit-base");
}

TEST_F(ContainerToolConfigTest, UserConfigOverridesSystemTool)
{
    using namespace linglong::ctk::detect;

    writeFile(tempDir.path() / "system.json",
              R"({
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base"
      ]
    }
  ]
})");
    writeFile(tempDir.path() / "user.json",
              R"({
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base-test",
        "libnvidia-container-tools"
      ]
    }
  ],
  "neverRemind": {
    "nvidia": true
  }
})");

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    ASSERT_EQ(manager.tools().size(), 1);
    EXPECT_EQ(manager.tools().front().deviceId, "nvidia");
    ASSERT_EQ(manager.tools().front().packages.size(), 2);
    EXPECT_EQ(manager.tools().front().packages[0], "nvidia-container-toolkit-base-test");
    EXPECT_EQ(manager.tools().front().packages[1], "libnvidia-container-tools");
    EXPECT_TRUE(manager.isNeverRemind("nvidia"));
}

TEST_F(ContainerToolConfigTest, EmptyUserToolsKeepsSystemTools)
{
    using namespace linglong::ctk::detect;

    writeFile(tempDir.path() / "system.json",
              R"({
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base"
      ]
    }
  ]
})");
    writeFile(tempDir.path() / "user.json", R"({"tools": []})");

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    ASSERT_EQ(manager.tools().size(), 1);
    EXPECT_EQ(manager.tools().front().deviceId, "nvidia");
}

TEST_F(ContainerToolConfigTest, ParsesAndDeduplicatesPackages)
{
    using namespace linglong::ctk::detect;

    writeFile(tempDir.path() / "system.json",
              R"({
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base",
        "",
        "libnvidia-container-tools",
        "nvidia-container-toolkit-base"
      ]
    }
  ]
})");

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    auto tools = manager.tools();
    ASSERT_EQ(tools.size(), 1);
    const auto &packages = tools.front().packages;
    ASSERT_EQ(packages.size(), 2);
    EXPECT_EQ(packages[0], "nvidia-container-toolkit-base");
    EXPECT_EQ(packages[1], "libnvidia-container-tools");
}

TEST_F(ContainerToolConfigTest, SaveUserConfigWritesNeverRemind)
{
    using namespace linglong::ctk::detect;

    ContainerToolConfigManager manager(tempDir.path() / "system.json",
                                       tempDir.path() / "user.json");
    ASSERT_TRUE(manager.load());
    manager.setNeverRemind("nvidia", true);
    ASSERT_TRUE(manager.saveUserConfig());

    ContainerToolConfigManager reloaded(tempDir.path() / "system.json",
                                        tempDir.path() / "user.json");
    ASSERT_TRUE(reloaded.load());
    EXPECT_TRUE(reloaded.isNeverRemind("nvidia"));
}
