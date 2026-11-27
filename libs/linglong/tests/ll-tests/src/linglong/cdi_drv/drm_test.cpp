// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/cdi_drv/drm.h"
#include "linglong/cdi_drv/types/DrmSelector.hpp"
#include "linglong/cdi_drv/types/Node.hpp"
#include "linglong/utils/file.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using linglong::cdi_drv::drmNodesSysfs;
using linglong::cdi_drv::types::DrmSelector;
using linglong::cdi_drv::types::Node;
using linglong::utils::writeFile;

class DrmNodesSysfsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(fs::create_directories(classDirectory));
        ASSERT_TRUE(fs::create_directories(deviceDirectory));
    }

    void addNode(const std::string &name,
                 const std::string &driver = "i915",
                 const std::string &vendor = "0x8086\n",
                 const std::string &device = "0x1234\n",
                 const std::string &subsystem = "pci")
    {
        const auto parent = temporary.path() / "devices" / name;
        ASSERT_TRUE(fs::create_directories(parent));
        ASSERT_TRUE(fs::create_directory(classDirectory / name));
        fs::create_directory_symlink(parent, classDirectory / name / "device");
        fs::create_symlink(fs::path("/sys/bus") / subsystem, parent / "subsystem");
        if (!driver.empty()) {
            fs::create_symlink(fs::path("/sys/bus") / subsystem / "drivers" / driver,
                               parent / "driver");
        }
        ASSERT_TRUE(writeFile(parent / "vendor", vendor));
        ASSERT_TRUE(writeFile(parent / "device", device));
        fs::create_symlink("/dev/null", deviceDirectory / name);
    }

    std::set<std::string> paths(std::initializer_list<const char *> names) const
    {
        std::set<std::string> result;
        for (const auto *name : names) {
            result.insert((deviceDirectory / name).string());
        }
        return result;
    }

    TempDir temporary;
    const fs::path classDirectory = temporary.path() / "sys/class/drm";
    const fs::path deviceDirectory = temporary.path() / "dev/dri";
};

TEST_F(DrmNodesSysfsTest, EnumeratesOnlyExistingCharacterDrmNodes)
{
    addNode("card0");
    addNode("renderD128");
    addNode("card0-DP-1");
    addNode("card");
    addNode("renderD");
    addNode("card1");
    ASSERT_TRUE(fs::remove(deviceDirectory / "card1"));
    ASSERT_TRUE(writeFile(deviceDirectory / "card1", "ordinary file"));
    addNode("card2");
    ASSERT_TRUE(fs::remove(deviceDirectory / "card2"));

    const auto result = drmNodesSysfs({}, classDirectory, deviceDirectory);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, paths({ "card0", "renderD128" }));
}

TEST_F(DrmNodesSysfsTest, FiltersNodeTypesAndAllowsEmptySelection)
{
    addNode("card0");
    addNode("renderD128");
    DrmSelector selector{};
    selector.nodes = std::vector<Node>{ Node::Render };
    const auto render = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(render);
    EXPECT_EQ(*render, paths({ "renderD128" }));

    selector.nodes = std::vector<Node>{ Node::Primary };
    const auto primary = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(primary);
    EXPECT_EQ(*primary, paths({ "card0" }));

    selector.nodes = std::vector<Node>{};
    selector.kernelDriver = "";
    const auto disabled = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(disabled);
    EXPECT_TRUE(disabled->empty());
}

TEST_F(DrmNodesSysfsTest, CombinesDriverAndPciFilters)
{
    addNode("card0");
    addNode("card1", "xe");
    addNode("card2", "i915", "0x8086\n", "0x5678\n");
    DrmSelector selector{};
    selector.kernelDriver = "i915";
    selector.pciIDS = std::vector<std::string>{ "8086:1234" };
    const auto exact = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(exact);
    EXPECT_EQ(*exact, paths({ "card0" }));

    selector.pciIDS = std::vector<std::string>{ "8086:*" };
    const auto wildcard = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(wildcard);
    EXPECT_EQ(*wildcard, paths({ "card0", "card2" }));
}

TEST_F(DrmNodesSysfsTest, MatchesPciWithoutDriverAndLeavesEmptyRulesUnrestricted)
{
    addNode("card0", "", "0xabcd\n", "0xef01\n");
    addNode("card1", "", "0xabcd\n", "0xef01\n", "platform");
    DrmSelector selector{};
    selector.pciIDS = std::vector<std::string>{ "ABCD:EF01" };
    const auto pci = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(pci);
    EXPECT_EQ(*pci, paths({ "card0" }));

    selector.pciIDS = std::vector<std::string>{};
    const auto unrestricted = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(unrestricted);
    EXPECT_EQ(*unrestricted, paths({ "card0", "card1" }));

    selector.kernelDriver = "i915";
    const auto unbound = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(unbound);
    EXPECT_TRUE(unbound->empty());
}

TEST_F(DrmNodesSysfsTest, SkipsMissingAttributesAndBrokenClassLinks)
{
    addNode("card0");
    addNode("card1");
    ASSERT_TRUE(fs::remove(temporary.path() / "devices/card1/vendor"));
    fs::create_directory_symlink("missing-device", classDirectory / "card2");
    DrmSelector selector{};
    selector.pciIDS = std::vector<std::string>{ "8086:*" };
    const auto result = drmNodesSysfs(selector, classDirectory, deviceDirectory);
    ASSERT_TRUE(result);
    EXPECT_EQ(*result, paths({ "card0" }));
}

TEST_F(DrmNodesSysfsTest, HandlesMissingDirectoryAndReportsEnumerationErrors)
{
    const auto missing = temporary.path() / "missing-drm";
    const auto empty = drmNodesSysfs({}, missing, deviceDirectory);
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
    ASSERT_TRUE(writeFile(missing, "not a directory"));
    EXPECT_FALSE(drmNodesSysfs({}, missing, deviceDirectory));

    DrmSelector selector{};
    selector.nodes = std::vector<Node>{};
    const auto disabled = drmNodesSysfs(selector, missing, deviceDirectory);
    ASSERT_TRUE(disabled);
    EXPECT_TRUE(disabled->empty());
}

} // namespace
