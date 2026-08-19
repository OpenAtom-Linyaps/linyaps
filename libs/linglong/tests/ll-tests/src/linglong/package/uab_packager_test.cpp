// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/layer_dir.h"
#include "linglong/package/uab_packager.h"

#include <filesystem>
#include <fstream>

namespace linglong::package {
namespace {

TEST(UABPackagerTest, CopyDirectoryForDistributedBundleCreatesHardLinks)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto source = tempDir.path() / "source";
    const auto destination = tempDir.path() / "destination";
    std::filesystem::create_directories(source / "usr/bin");

    const auto sourceFile = source / "usr/bin/program";
    std::ofstream{ sourceFile } << "content";

    auto result = detail::copyDirectoryForDistributedBundle(source, destination);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::is_directory(destination / "usr/bin"));
    EXPECT_TRUE(std::filesystem::equivalent(sourceFile, destination / "usr/bin/program"));
}

TEST(UABPackagerTest, CopyDirectoryForDistributedBundlePreservesSymlinks)
{
    TempDir tempDir("uab-packager-test-");
    ASSERT_TRUE(tempDir.isValid());

    const auto source = tempDir.path() / "source";
    const auto destination = tempDir.path() / "destination";
    std::filesystem::create_directories(source / "usr/bin");
    std::ofstream{ source / "usr/bin/program" } << "content";
    std::filesystem::create_symlink("program", source / "usr/bin/program-link");
    std::filesystem::create_directory_symlink("usr/bin", source / "bin");

    auto result = detail::copyDirectoryForDistributedBundle(source, destination);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::is_symlink(destination / "usr/bin/program-link"));
    EXPECT_EQ(std::filesystem::read_symlink(destination / "usr/bin/program-link"), "program");

    EXPECT_TRUE(std::filesystem::is_symlink(destination / "bin"));
    EXPECT_EQ(std::filesystem::read_symlink(destination / "bin"), "usr/bin");
}

TEST(UABPackagerTest, SetIconRejectsNonExistent)
{
    UABPackager packager("/tmp/project", "/tmp/build");
    auto result = packager.setIcon("/no/such/icon.png");
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("failed to check icon file status"));
}

TEST(UABPackagerTest, SetIconRejectsDirectory)
{
    TempDir td("uab-icon-test-");
    ASSERT_TRUE(td.isValid());
    UABPackager packager("/tmp/project", "/tmp/build");
    auto result = packager.setIcon(td.path());
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("icon isn\'t a file"));
}

TEST(UABPackagerTest, AppendLayerRejectsInvalid)
{
    TempDir td("uab-layer-test-");
    ASSERT_TRUE(td.isValid());
    LayerDir invalid(td.path() / "no-layer");
    UABPackager packager("/tmp/project", "/tmp/build");
    auto result = packager.appendLayer(invalid);
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("invalid layer directory"));
}

TEST(UABPackagerTest, ExcludeIncludeRejectsNonAbsolute)
{
    UABPackager packager("/tmp/project", "/tmp/build");
    auto excl = packager.exclude({ "relative/path" });
    ASSERT_FALSE(excl.has_value());
    EXPECT_THAT(excl.error().message(), ::testing::HasSubstr("invalid format"));

    auto incl = packager.include({ "also/relative" });
    ASSERT_FALSE(incl.has_value());
    EXPECT_THAT(incl.error().message(), ::testing::HasSubstr("invalid format"));
}

TEST(UABPackagerTest, PackEndToEndWithValidLayers)
{
    // /proc/self/exe serves as a valid ELF header.
    TempDir headerDir("uab-header-");
    ASSERT_TRUE(headerDir.isValid());
    auto header = headerDir.path() / "header.elf";
    std::filesystem::copy_file("/proc/self/exe", header);

    // Build a base layer (kind=base).
    TempDir baseDir("uab-base-");
    ASSERT_TRUE(baseDir.isValid());
    std::filesystem::create_directories(baseDir.path() / "files" / "usr" / "lib");
    std::ofstream{ baseDir.path() / "files" / "usr" / "lib" / "libbase.so" } << "base";
    {
        api::types::v1::PackageInfoV2 baseInfo;
        baseInfo.id = "org.deepin.base";
        baseInfo.version = "1";
        baseInfo.arch = { "x86_64" };
        baseInfo.kind = "base";
        baseInfo.packageInfoV2Module = "binary";
        baseInfo.channel = "main";
        nlohmann::json j = baseInfo;
        std::ofstream{ baseDir.path() / "info.json" } << j.dump();
    }

    // Build an app layer (kind=app).
    TempDir appDir("uab-app-");
    ASSERT_TRUE(appDir.isValid());
    std::filesystem::create_directories(appDir.path() / "files" / "usr" / "bin");
    std::ofstream{ appDir.path() / "files" / "usr" / "bin" / "hello" } << "hello world";
    {
        api::types::v1::PackageInfoV2 appInfo;
        appInfo.id = "com.example.app";
        appInfo.version = "1.0.0";
        appInfo.arch = { "x86_64" };
        appInfo.kind = "app";
        appInfo.packageInfoV2Module = "binary";
        appInfo.channel = "main";
        nlohmann::json j = appInfo;
        std::ofstream{ appDir.path() / "info.json" } << j.dump();
    }

    LayerDir baseLayer(baseDir.path());
    ASSERT_TRUE(baseLayer.valid());

    LayerDir appLayer(appDir.path());
    ASSERT_TRUE(appLayer.valid());

    // Pack.
    TempDir buildDir("uab-build-");
    ASSERT_TRUE(buildDir.isValid());
    TempDir workDir("uab-work-");
    ASSERT_TRUE(workDir.isValid());

    UABPackager packager(workDir.path(), buildDir.path());
    packager.setDefaultHeader(header);
    packager.setCompressor("lz4");
    // A custom loader avoids the compile-time default path lookup.
    auto loaderFile = buildDir.path() / "uab-loader-dummy";
    std::ofstream{ loaderFile } << "dummy loader";
    packager.setLoader(loaderFile);
    ASSERT_TRUE(packager.appendLayer(baseLayer).has_value());
    ASSERT_TRUE(packager.appendLayer(appLayer).has_value());

    auto outPath = buildDir.path() / "output.uab";
    auto result = packager.pack(outPath, false);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_TRUE(std::filesystem::exists(outPath));
    EXPECT_GT(std::filesystem::file_size(outPath), 0);
}

} // namespace

} // namespace linglong::package
