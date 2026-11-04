/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/architecture.h"
#include "linglong/package/elf_handler.h"
#include "linglong/package/fallback_version.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/layer_packager.h"
#include "linglong/package/uab_packager.h"
#include "linglong/package/version.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace linglong::package {
namespace {

class PackagerElfDeepTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_TRUE(tempDir.isValid()); }

    TempDir tempDir{ "packager-elf-deep-test-" };
};

TEST_F(PackagerElfDeepTest, LayerDirMetadataAndValidationFlow)
{
    auto layerPath = tempDir.path() / "test-layer";
    std::filesystem::create_directories(layerPath / "files");

    api::types::v1::PackageInfoV2 info;
    info.id = "org.deepin.testapp";
    info.version = "1.2.3";
    info.arch = { "x86_64" };
    info.kind = "app";
    info.packageInfoV2Module = "binary";
    info.channel = "main";
    nlohmann::json j = info;
    std::ofstream{ layerPath / "info.json" } << j.dump();

    LayerDir layer(layerPath);
    EXPECT_TRUE(layer.valid());
    EXPECT_EQ(layer.info().id, "org.deepin.testapp");
    EXPECT_EQ(layer.info().version, "1.2.3");
}

TEST_F(PackagerElfDeepTest, LayerDirInvalidJsonRejection)
{
    auto layerPath = tempDir.path() / "bad-json-layer";
    std::filesystem::create_directories(layerPath / "files");
    std::ofstream{ layerPath / "info.json" } << "{ broken json content... ";

    LayerDir layer(layerPath);
    EXPECT_FALSE(layer.valid());
}

TEST_F(PackagerElfDeepTest, LayerDirMissingFilesDirectoryRejection)
{
    auto layerPath = tempDir.path() / "no-files-layer";
    std::filesystem::create_directories(layerPath);

    api::types::v1::PackageInfoV2 info;
    info.id = "org.deepin.nofiles";
    info.version = "1.0.0";
    info.arch = { "x86_64" };
    info.kind = "app";
    nlohmann::json j = info;
    std::ofstream{ layerPath / "info.json" } << j.dump();

    LayerDir layer(layerPath);
    EXPECT_FALSE(layer.valid());
}

TEST_F(PackagerElfDeepTest, UABPackagerCompressorOptionSettings)
{
    UABPackager packager(tempDir.path());

    EXPECT_NO_THROW(packager.setCompressor("zstd"));
    EXPECT_NO_THROW(packager.setCompressor("lz4"));
    EXPECT_NO_THROW(packager.setCompressor("gzip"));
}

TEST_F(PackagerElfDeepTest, UABPackagerDuplicateLayerPrevention)
{
    auto layerPath = tempDir.path() / "dup-layer";
    std::filesystem::create_directories(layerPath / "files");

    api::types::v1::PackageInfoV2 info;
    info.id = "org.deepin.dup";
    info.version = "1.0.0";
    info.arch = { "x86_64" };
    info.kind = "app";
    nlohmann::json j = info;
    std::ofstream{ layerPath / "info.json" } << j.dump();

    LayerDir layer(layerPath);
    ASSERT_TRUE(layer.valid());

    UABPackager packager(tempDir.path());
    auto r1 = packager.appendLayer(layer);
    EXPECT_TRUE(r1.has_value());

    auto r2 = packager.appendLayer(layer);
    EXPECT_TRUE(r2.has_value());
}

TEST_F(PackagerElfDeepTest, FallbackVersionParsingAndFormatting)
{
    auto v1 = FallbackVersion::parse("1.2.3.4");
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1->toString(), "1.2.3.4");

    auto v2 = FallbackVersion::parse("1.0");
    ASSERT_TRUE(v2.has_value());

    EXPECT_TRUE(*v1 > *v2);
    EXPECT_TRUE(*v2 < *v1);
}

TEST_F(PackagerElfDeepTest, FallbackVersionComparisonMatrix)
{
    auto vA = FallbackVersion::parse("2.0.0");
    auto vB = FallbackVersion::parse("2.0.0");
    auto vC = FallbackVersion::parse("2.0.1");

    ASSERT_TRUE(vA && vB && vC);
    EXPECT_EQ(*vA, *vB);
    EXPECT_NE(*vA, *vC);
    EXPECT_LT(*vA, *vC);
}

TEST_F(PackagerElfDeepTest, ArchitectureEnumParsingAndToString)
{
    auto arch1 = Architecture::parse("x86_64");
    ASSERT_TRUE(arch1.has_value());
    EXPECT_EQ(Architecture::toString(*arch1), "x86_64");

    auto arch2 = Architecture::parse("aarch64");
    ASSERT_TRUE(arch2.has_value());
    EXPECT_EQ(Architecture::toString(*arch2), "aarch64");

    auto archBad = Architecture::parse("unknown_arch_123");
    EXPECT_FALSE(archBad.has_value());
}

TEST_F(PackagerElfDeepTest, ExecEntryEscapingSpecialCharactersInArgs)
{
    auto entry = detail::generateExecEntry(
      { "/opt/apps/demo/files/bin/demo", "--msg=hello world", "$SPECIAL_VAR", "`id`" },
      "/opt/apps/demo/files");
    ASSERT_TRUE(entry.has_value()) << entry.error().message();

    EXPECT_NE(entry->find("--msg=hello world"), std::string::npos);
}

TEST_F(PackagerElfDeepTest, LayerDirMultiArchitectureSupport)
{
    auto layerPath = tempDir.path() / "multi-arch-layer";
    std::filesystem::create_directories(layerPath / "files");

    api::types::v1::PackageInfoV2 info;
    info.id = "org.deepin.multiarch";
    info.version = "1.0.0";
    info.arch = { "x86_64", "aarch64", "loongarch64" };
    info.kind = "app";
    nlohmann::json j = info;
    std::ofstream{ layerPath / "info.json" } << j.dump();

    LayerDir layer(layerPath);
    EXPECT_TRUE(layer.valid());
    EXPECT_EQ(layer.info().arch.size(), 3U);
}

TEST_F(PackagerElfDeepTest, UABPackagerIconFileCheck)
{
    auto iconPath = tempDir.path() / "icon.png";
    std::ofstream{ iconPath } << "fake png data";

    UABPackager packager(tempDir.path());
    auto res = packager.setIcon(iconPath.string());
    EXPECT_TRUE(res.has_value()) << res.error().message();
}

TEST_F(PackagerElfDeepTest, CopyDirectoryWithNestedSymlinksAndPermissions)
{
    const auto source = tempDir.path() / "nested_src";
    const auto destination = tempDir.path() / "nested_dst";
    std::filesystem::create_directories(source / "a" / "b" / "c");

    std::ofstream{ source / "a" / "b" / "c" / "file.txt" } << "nested data";
    std::filesystem::create_symlink("c/file.txt", source / "a" / "b" / "link.txt");

    auto result = detail::copyDirectoryForDistributedBundle(source, destination);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::exists(destination / "a" / "b" / "c" / "file.txt"));
    EXPECT_TRUE(std::filesystem::is_symlink(destination / "a" / "b" / "link.txt"));
}

TEST_F(PackagerElfDeepTest, ExecEntryRelativePathResolutionTest)
{
    auto entry1 = detail::generateExecEntry({ "./bin/app" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry1.has_value());

    auto entry2 = detail::generateExecEntry({ "bin/app" }, "/opt/apps/demo/files");
    ASSERT_TRUE(entry2.has_value());
}

} // namespace
} // namespace linglong::package
