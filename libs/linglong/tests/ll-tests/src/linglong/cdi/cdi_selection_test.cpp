// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/cdi/cdi.h"

#include <common/tempdir.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace {

class CDISelectionTest : public ::testing::Test
{
protected:
    void writeSpec(const std::filesystem::path &path, const std::string &hook)
    {
        nlohmann::json spec = {
            { "cdiVersion", "0.6.0" },
            { "kind", "vendor.example/gpu" },
            { "containerEdits",
              { { "hooks", { { { "hookName", "createContainer" }, { "path", hook } } } },
                { "mounts", { { { "hostPath", "/vendor" }, { "containerPath", "/vendor" } } } } } },
            { "devices",
              { { { "name", "gpu0" }, { "containerEdits", { { "env", { "GPU0=1" } } } } },
                { { "name", "gpu1" }, { "containerEdits", { { "env", { "GPU1=1" } } } } } } }
        };
        std::ofstream(path) << spec.dump();
    }

    void SetUp() override
    {
        writeSpec(dir.path() / "gpu.json", "/vendor/setup");
        auto found = linglong::cdi::getCDIDevices({ dir.path().string() }, std::nullopt);
        ASSERT_TRUE(found) << found.error().message();
        devices = std::move(*found);
        ASSERT_EQ(devices.size(), 2U);
        std::sort(devices.begin(), devices.end(), [](const auto &a, const auto &b) {
            return a.name < b.name;
        });
    }

    TempDir dir{ "ll-cdi-selection-" };
    std::vector<linglong::api::types::v1::CdiDeviceEntry> devices;
};

TEST_F(CDISelectionTest, AppliesSpecEditsOnceAndPreservesBothDevices)
{
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_TRUE(edits) << edits.error().message();
    ASSERT_TRUE(edits->hooks);
    ASSERT_EQ(edits->hooks->size(), 1U);
    EXPECT_EQ(edits->hooks->front().path, "/vendor/setup");
    ASSERT_TRUE(edits->mounts);
    EXPECT_EQ(edits->mounts->size(), 1U);
    ASSERT_TRUE(edits->env);
    EXPECT_EQ(*edits->env, (std::vector<std::string>{ "GPU0=1", "GPU1=1" }));
}

TEST_F(CDISelectionTest, KeepsGlobalsFromDistinctSpecsOfSameKind)
{
    auto second = dir.path() / "second.json";
    writeSpec(second, "/vendor/second-setup");
    devices[1].spec.path = second.string();
    devices[1].spec.checksum.clear();
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_TRUE(edits) << edits.error().message();
    ASSERT_TRUE(edits->hooks);
    ASSERT_EQ(edits->hooks->size(), 2U);
    EXPECT_EQ(edits->hooks->at(1).path, "/vendor/second-setup");
}

TEST_F(CDISelectionTest, RecognizesSymlinkAliasesOfSameSpec)
{
    auto alias = dir.path() / "alias.json";
    std::filesystem::create_symlink(dir.path() / "gpu.json", alias);
    devices[1].spec.path = alias.string();
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_TRUE(edits) << edits.error().message();
    ASSERT_TRUE(edits->hooks);
    EXPECT_EQ(edits->hooks->size(), 1U);
}

TEST_F(CDISelectionTest, ChecksChecksumOfEveryEntry)
{
    devices[1].spec.checksum = "incorrect";
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_FALSE(edits);
    EXPECT_NE(edits.error().message().find("checksum mismatch"), std::string::npos);
}

TEST_F(CDISelectionTest, ChecksKindOfEveryEntry)
{
    devices[1].kind = "other.example/gpu";
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_FALSE(edits);
    EXPECT_NE(edits.error().message().find("kind mismatch"), std::string::npos);
}

TEST_F(CDISelectionTest, ChecksNameOfEveryEntry)
{
    devices[1].name = "missing";
    auto edits = linglong::cdi::getCDIDevicesEdits(devices);
    ASSERT_FALSE(edits);
    EXPECT_NE(edits.error().message().find("device not found"), std::string::npos);
}

TEST_F(CDISelectionTest, SingleDeviceRetainsSpecAndDeviceEdits)
{
    auto edits = linglong::cdi::getCDIDeviceEdits(devices.front());
    ASSERT_TRUE(edits) << edits.error().message();
    ASSERT_TRUE(edits->hooks);
    EXPECT_EQ(edits->hooks->size(), 1U);
    ASSERT_TRUE(edits->env);
    EXPECT_EQ(*edits->env, (std::vector<std::string>{ "GPU0=1" }));
}

TEST_F(CDISelectionTest, EmptySelectionHasNoEdits)
{
    auto edits = linglong::cdi::getCDIDevicesEdits({});
    ASSERT_TRUE(edits) << edits.error().message();
    EXPECT_FALSE(edits->hooks);
    EXPECT_FALSE(edits->env);
    EXPECT_FALSE(edits->mounts);
}

} // namespace
