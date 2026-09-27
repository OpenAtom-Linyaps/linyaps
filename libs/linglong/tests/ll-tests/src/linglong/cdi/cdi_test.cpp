// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/cdi/cdi.h"
#include "linglong/cdi/types/Cdi.hpp"
#include "linglong/cdi/types/Generators.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using linglong::api::types::v1::CdiDeviceEntry;
using linglong::cdi::getCDIDeviceEdits;
using linglong::cdi::getCDIDevices;

namespace {

constexpr auto kKind = "vendor.com/gpu";

// A minimal but valid CDI spec: two devices, global env merged with per-device edits.
std::string specJSON(const std::string &kind = kKind)
{
    return R"({
  "cdiVersion": "0.7.0",
  "kind": ")"
      + kind + R"(",
  "containerEdits": {
    "env": ["GLOBAL_ENV=1"]
  },
  "devices": [
    {
      "name": "dev0",
      "containerEdits": {
        "env": ["DEV0_ENV=1"],
        "deviceNodes": [{"path": "/dev/vendor-gpu0"}]
      }
    },
    {
      "name": "dev1",
      "containerEdits": {
        "env": ["DEV1_ENV=1"]
      }
    }
  ]
})";
}

fs::path writeSpec(const TempDir &dir, const std::string &filename, const std::string &content)
{
    auto path = dir.path() / filename;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

bool isLowerHex64(const std::string &s)
{
    if (s.size() != 64) {
        return false;
    }
    for (char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(CDITest, GetAllDevicesFromJSONSpec)
{
    TempDir dir;
    auto specPath = writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices = getCDIDevices({ dir.path().string() }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_EQ(devices->size(), 2U);

    // directory_iterator order is unspecified; collect names before asserting.
    std::vector<std::string> names{ (*devices)[0].name, (*devices)[1].name };
    std::sort(names.begin(), names.end());
    EXPECT_EQ(names[0], "dev0");
    EXPECT_EQ(names[1], "dev1");

    for (const auto &dev : *devices) {
        EXPECT_EQ(dev.kind, kKind);
        EXPECT_EQ(dev.spec.path, specPath.string());
        EXPECT_TRUE(isLowerHex64(dev.spec.checksum)) << "checksum: " << dev.spec.checksum;
    }
}

TEST(CDITest, ParseYAMLSpec)
{
    TempDir dir;
    writeSpec(dir,
              "vendor-gpu.yaml",
              R"(cdiVersion: "0.7.0"
kind: vendor.com/gpu
devices:
  - name: dev0
    containerEdits:
      env:
        - DEV0_ENV=1
)");

    auto devices = getCDIDevices({ dir.path().string() }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_EQ(devices->size(), 1U);
    EXPECT_EQ((*devices)[0].name, "dev0");
    EXPECT_EQ((*devices)[0].kind, kKind);
}

TEST(CDITest, SkipUnparsableAndWrongExtensionSpecs)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());
    writeSpec(dir, "broken.json", "{ not valid json");
    writeSpec(dir, "notes.txt", "kind: whatever");

    // Unparsable / wrong-extension files are skipped, not fatal.
    auto devices = getCDIDevices({ dir.path().string() }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_EQ(devices->size(), 2U);
    for (const auto &dev : *devices) {
        EXPECT_EQ(dev.spec.path, (dir.path() / "vendor-gpu.json").string());
    }
}

TEST(CDITest, NonexistentSpecDirYieldsEmpty)
{
    TempDir dir;
    auto missing = (dir.path() / "does-not-exist").string();

    auto devices = getCDIDevices({ missing }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    EXPECT_TRUE(devices->empty());
}

TEST(CDITest, SelectDeviceByKindEqualsName)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices =
      getCDIDevices({ dir.path().string() },
                    std::optional<std::vector<std::string>>{ { "vendor.com/gpu=dev1" } });
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_EQ(devices->size(), 1U);
    EXPECT_EQ((*devices)[0].name, "dev1");
    EXPECT_EQ((*devices)[0].kind, kKind);
}

TEST(CDITest, SelectDeviceInvalidFormat)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices = getCDIDevices({ dir.path().string() },
                                 std::optional<std::vector<std::string>>{ { "no-equals-sign" } });
    ASSERT_FALSE(devices.has_value());
    EXPECT_NE(devices.error().message().find("invalid device format"), std::string::npos)
      << devices.error().message();
}

TEST(CDITest, SelectDeviceNotFound)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices =
      getCDIDevices({ dir.path().string() },
                    std::optional<std::vector<std::string>>{ { "vendor.com/gpu=nope" } });
    ASSERT_FALSE(devices.has_value());
    EXPECT_NE(devices.error().message().find("device not found"), std::string::npos)
      << devices.error().message();
}

TEST(CDITest, DeviceEditsMergeGlobalAndLocal)
{
    TempDir dir;
    auto specPath = writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices =
      getCDIDevices({ dir.path().string() },
                    std::optional<std::vector<std::string>>{ { "vendor.com/gpu=dev0" } });
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_EQ(devices->size(), 1U);

    auto edits = getCDIDeviceEdits((*devices)[0]);
    ASSERT_TRUE(edits.has_value()) << edits.error().message();

    // Global env first, then device-local env (mergeEdits appends local to global).
    ASSERT_TRUE(edits->env.has_value());
    ASSERT_EQ(edits->env->size(), 2U);
    EXPECT_EQ((*edits->env)[0], "GLOBAL_ENV=1");
    EXPECT_EQ((*edits->env)[1], "DEV0_ENV=1");

    ASSERT_TRUE(edits->deviceNodes.has_value());
    ASSERT_EQ(edits->deviceNodes->size(), 1U);
    EXPECT_EQ((*edits->deviceNodes)[0].path, "/dev/vendor-gpu0");

    EXPECT_FALSE(edits->hooks.has_value());
    EXPECT_FALSE(edits->intelRdt.has_value());
}

TEST(CDITest, DeviceEditsChecksumMismatch)
{
    TempDir dir;
    auto specPath = writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices =
      getCDIDevices({ dir.path().string() },
                    std::optional<std::vector<std::string>>{ { "vendor.com/gpu=dev0" } });
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    auto entry = (*devices)[0];

    // Tamper with the spec after its checksum was recorded.
    writeSpec(dir, "vendor-gpu.json", specJSON("other.com/gpu"));

    auto edits = getCDIDeviceEdits(entry);
    ASSERT_FALSE(edits.has_value());
    EXPECT_NE(edits.error().message().find("checksum mismatch"), std::string::npos)
      << edits.error().message();
}

TEST(CDITest, DeviceEditsKindMismatch)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices = getCDIDevices({ dir.path().string() }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_FALSE(devices->empty());

    auto entry = (*devices)[0];
    entry.kind = "spoofed.com/device";

    auto edits = getCDIDeviceEdits(entry);
    ASSERT_FALSE(edits.has_value());
    EXPECT_NE(edits.error().message().find("kind mismatch"), std::string::npos)
      << edits.error().message();
}

TEST(CDITest, DeviceEditsUnknownDeviceName)
{
    TempDir dir;
    writeSpec(dir, "vendor-gpu.json", specJSON());

    auto devices = getCDIDevices({ dir.path().string() }, std::nullopt);
    ASSERT_TRUE(devices.has_value()) << devices.error().message();
    ASSERT_FALSE(devices->empty());

    auto entry = (*devices)[0];
    entry.name = "ghost-device";

    auto edits = getCDIDeviceEdits(entry);
    ASSERT_FALSE(edits.has_value());
    EXPECT_NE(edits.error().message().find("device not found"), std::string::npos)
      << edits.error().message();
}

TEST(CDITest, DeviceEditsFromMissingSpecFile)
{
    CdiDeviceEntry entry;
    entry.kind = kKind;
    entry.name = "dev0";
    entry.spec.path = "/nonexistent/dir/spec.json";
    entry.spec.checksum = ""; // empty checksum skips the mismatch check

    auto edits = getCDIDeviceEdits(entry);
    ASSERT_FALSE(edits.has_value());
    EXPECT_NE(edits.error().message().find("failed to open file"), std::string::npos)
      << edits.error().message();
}
