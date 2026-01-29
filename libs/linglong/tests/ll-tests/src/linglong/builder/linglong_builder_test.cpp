// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "../mocks/linglong_builder_mock.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <unistd.h>

TEST(LinglongBuilder, installModule)
{
    TempDir buildOutput;
    ASSERT_TRUE(buildOutput.isValid());
    TempDir moduleOutput;
    ASSERT_TRUE(moduleOutput.isValid());

    // Create test files and directories
    const auto &buildPath = buildOutput.path();
    std::filesystem::create_directories(buildPath / "include");
    std::filesystem::create_directories(buildPath / "lib/debug");
    std::filesystem::create_directories(buildPath / "bin");
    std::filesystem::create_directories(buildPath / "share/icons/hicolor/48x48/apps");

    std::ofstream(buildPath / "include/header.h");
    std::ofstream(buildPath / "lib/libfoo.a");
    std::ofstream(buildPath / "lib/libfoo.so");
    std::ofstream(buildPath / "lib/debug/libfoo.so.debug");
    std::ofstream(buildPath / "bin/tool");
    std::ofstream(buildPath / "share/app.desktop");
    std::ofstream(buildPath / "share/icons/hicolor/48x48/apps/app.png");

    // Define install rules
    std::unordered_set<std::string> rules = {
        "^/include/.+",  // regex rule
        "/lib/libfoo.a", // normal path rule
        "/bin/tool",
        "^/share/icons/.+", // regex rule for icons
        "# a comment",      // comment rule
        ""                  // empty rule
    };

    auto result = linglong::builder::installModule(buildOutput.path(), moduleOutput.path(), rules);
    ASSERT_TRUE(result.has_value());

    const auto &modulePath = moduleOutput.path();
    // check files are moved
    EXPECT_TRUE(std::filesystem::exists(modulePath / "include/header.h"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "lib/libfoo.a"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "bin/tool"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "share/icons/hicolor/48x48/apps/app.png"));

    // check files are NOT moved
    EXPECT_FALSE(std::filesystem::exists(modulePath / "lib/libfoo.so"));
    EXPECT_FALSE(std::filesystem::exists(modulePath / "lib/debug/libfoo.so.debug"));
    EXPECT_FALSE(std::filesystem::exists(modulePath / "share/app.desktop"));

    // check original files are removed (because they are moved)
    EXPECT_FALSE(std::filesystem::exists(buildPath / "include/header.h"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "lib/libfoo.a"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "bin/tool"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "share/icons/hicolor/48x48/apps/app.png"));

    // check files that should not be moved are still there
    EXPECT_TRUE(std::filesystem::exists(buildPath / "lib/libfoo.so"));
    EXPECT_TRUE(std::filesystem::exists(buildPath / "lib/debug/libfoo.so.debug"));
    EXPECT_TRUE(std::filesystem::exists(buildPath / "share/app.desktop"));

    // Check returned installed file list
    std::vector<std::string> expectedFiles = { "bin",
                                               "bin/tool",
                                               "include",
                                               "include/header.h",
                                               "lib",
                                               "lib/libfoo.a",
                                               "share",
                                               "share/icons",
                                               "share/icons/hicolor",
                                               "share/icons/hicolor/48x48",
                                               "share/icons/hicolor/48x48/apps",
                                               "share/icons/hicolor/48x48/apps/app.png" };
    std::vector<std::string> installedFiles;
    for (const auto &p : *result) {
        installedFiles.emplace_back(p);
    }
    std::sort(installedFiles.begin(), installedFiles.end());

    std::sort(expectedFiles.begin(), expectedFiles.end());

    EXPECT_EQ(installedFiles, expectedFiles);
}

TEST(LinglongBuilder, mergeOutput)
{
    TempDir srcDir1;
    ASSERT_TRUE(srcDir1.isValid());
    TempDir srcDir2;
    ASSERT_TRUE(srcDir2.isValid());
    TempDir destDir;
    ASSERT_TRUE(destDir.isValid());

    // Create test files and directories
    const auto &srcPath1 = srcDir1.path();
    std::filesystem::create_directories(srcPath1 / "bin");
    std::filesystem::create_directories(srcPath1 / "lib");
    std::filesystem::create_directories(srcPath1 / "share");

    std::ofstream(srcPath1 / "bin/tool1");
    std::ofstream(srcPath1 / "lib/libfoo.so");
    std::ofstream(srcPath1 / "share/data1");
    std::ofstream(srcPath1 / ".wh.somefile");

    const auto &srcPath2 = srcDir2.path();
    std::filesystem::create_directories(srcPath2 / "bin");
    std::filesystem::create_directories(srcPath2 / "lib");
    std::filesystem::create_directories(srcPath2 / "share");

    std::ofstream(srcPath2 / "bin/tool2");
    std::ofstream(srcPath2 / "lib/libbar.so");
    std::ofstream(srcPath2 / "share/data2");

    // Define merge targets
    std::vector<std::string> targets = { "bin/", "share/data1" };

    linglong::builder::detail::mergeOutput({ srcPath1, srcPath2 }, destDir.path(), targets);

    const auto &destPath = destDir.path();
    // Check files are merged
    EXPECT_TRUE(std::filesystem::exists(destPath / "bin/tool1"));
    EXPECT_TRUE(std::filesystem::exists(destPath / "bin/tool2"));
    EXPECT_TRUE(std::filesystem::exists(destPath / "share/data1"));

    // Check files are NOT merged
    EXPECT_FALSE(std::filesystem::exists(destPath / "lib/libfoo.so"));
    EXPECT_FALSE(std::filesystem::exists(destPath / "lib/libbar.so"));
    EXPECT_FALSE(std::filesystem::exists(destPath / "share/data2"));
    EXPECT_FALSE(std::filesystem::exists(destPath / ".wh.somefile"));
}

TEST(LinglongBuilder, UabExportFilename)
{
    linglong::builder::BuilderMock builder;
    auto ref =
      linglong::package::Reference::parse(std::string("main:org.deepin.demo/1.0.0.0/arm64"));
    EXPECT_TRUE(ref.has_value()) << ref.error().message();
    auto filename = builder.uabExportFilename(*ref);
    EXPECT_EQ(filename, "org.deepin.demo_1.0.0.0_arm64_main.uab");
};

TEST(LinglongBuilder, LayerExportFilename)
{
    linglong::builder::BuilderMock builder;
    auto ref =
      linglong::package::Reference::parse(std::string("main:org.deepin.demo/1.0.0.0/arm64"));
    EXPECT_TRUE(ref.has_value()) << ref.error().message();
    auto filename = builder.layerExportFilename(*ref, "binary");
    EXPECT_EQ(filename, "org.deepin.demo_1.0.0.0_arm64_binary.layer");
};
