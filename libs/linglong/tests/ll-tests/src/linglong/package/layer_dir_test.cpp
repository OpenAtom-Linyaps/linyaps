// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/package/layer_dir.h"

#include <common/tempdir.h>

#include <filesystem>
#include <fstream>

using namespace linglong::package;

class LayerDirTest : public ::testing::Test
{
protected:
    void SetUp() override { testDir = std::make_unique<TempDir>("layer_dir_test"); }

    void TearDown() override { }

    std::unique_ptr<TempDir> testDir;
};

TEST_F(LayerDirTest, PathMethod)
{
    std::filesystem::path testPath = testDir->path() / "another_layer";
    LayerDir layerDir(testPath);

    EXPECT_EQ(layerDir.path(), testPath);
}

TEST_F(LayerDirTest, ValidWithInfoJson)
{
    std::filesystem::path layerPath = testDir->path() / "valid_layer";
    std::filesystem::create_directories(layerPath);

    std::ofstream infoFile(layerPath / "info.json");
    infoFile << R"({
        "id": "com.example.TestApp",
        "version": "1.0.0",
        "arch": ["x86_64"]
    })";
    infoFile.close();

    LayerDir layerDir(layerPath);
    EXPECT_TRUE(layerDir.valid());
}

TEST_F(LayerDirTest, InvalidWithoutInfoJson)
{
    std::filesystem::path layerPath = testDir->path() / "invalid_layer";
    std::filesystem::create_directories(layerPath);

    LayerDir layerDir(layerPath);
    EXPECT_FALSE(layerDir.valid());
}

TEST_F(LayerDirTest, ValidWithNonExistingDirectory)
{
    std::filesystem::path nonExistentPath = testDir->path() / "non_existent";
    LayerDir layerDir(nonExistentPath);

    EXPECT_FALSE(layerDir.valid());
}

TEST_F(LayerDirTest, FilesDirPath)
{
    std::filesystem::path layerPath = testDir->path() / "layer_with_files";
    LayerDir layerDir(layerPath);

    std::filesystem::path expectedFilesPath = layerPath / "files";
    EXPECT_EQ(layerDir.filesDirPath(), expectedFilesPath);
}

TEST_F(LayerDirTest, InfoWithValidJson)
{
    std::filesystem::path layerPath = testDir->path() / "layer_with_info";
    std::filesystem::create_directories(layerPath);

    std::ofstream infoFile(layerPath / "info.json");
    infoFile << R"({
        "arch": ["x86_64"],
        "base": "base",
        "channel": "main",
        "id": "com.example.TestApp",
        "kind": "app",
        "module": "binary",
        "name": "TestApp",
        "schema_version": "1.0",
        "size": 100,
        "version": "1.0.0"
    })";
    infoFile.close();

    LayerDir layerDir(layerPath);
    auto result = layerDir.info();

    EXPECT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->arch.size(), 1);
    EXPECT_EQ(result->arch[0], "x86_64");
    EXPECT_EQ(result->base, "base");
    EXPECT_EQ(result->channel, "main");
    EXPECT_EQ(result->id, "com.example.TestApp");
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->version, "1.0.0");
}

TEST_F(LayerDirTest, InfoWithMissingInfoJson)
{
    std::filesystem::path layerPath = testDir->path() / "layer_missing_info";
    std::filesystem::create_directories(layerPath);

    LayerDir layerDir(layerPath);
    auto result = layerDir.info();

    EXPECT_FALSE(result.has_value());
}

TEST_F(LayerDirTest, InfoWithInvalidJson)
{
    std::filesystem::path layerPath = testDir->path() / "layer_invalid_info";
    std::filesystem::create_directories(layerPath);

    std::ofstream infoFile(layerPath / "info.json");
    infoFile << R"({
        "id": "com.example.TestApp",
        "version": "1.0.0",
        "arch": ["x86_64",
        // Invalid JSON - missing closing bracket
    })";
    infoFile.close();

    LayerDir layerDir(layerPath);
    auto result = layerDir.info();

    EXPECT_FALSE(result.has_value());
}

TEST_F(LayerDirTest, InfoWithEmptyJson)
{
    std::filesystem::path layerPath = testDir->path() / "layer_empty_info";
    std::filesystem::create_directories(layerPath);

    std::ofstream infoFile(layerPath / "info.json");
    infoFile << "{}";
    infoFile.close();

    LayerDir layerDir(layerPath);
    auto result = layerDir.info();

    EXPECT_FALSE(result.has_value());
}
