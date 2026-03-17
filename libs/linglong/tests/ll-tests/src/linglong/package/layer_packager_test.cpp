// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../mocks/layer_packager_mock.h"
#include "common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/strings.h"
#include "linglong/package/layer_packager.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QSharedPointer>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace linglong;

namespace linglong::package {

class LayerPackagerTest : public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        auto layerTempDir =
          std::make_unique<TempDir>("linglong-layer-packager-test-SetUpTestSuite-");
        ASSERT_TRUE(layerTempDir->isValid()) << "Failed to create temporary directory";
        const auto &layerDirPath = layerTempDir->path();
        // 创建临时文件，用于之后打包测试
        std::filesystem::create_directories(layerDirPath / "files");
        std::string helloFilePath = layerDirPath / "files" / "hello";
        std::ofstream tmpFile(helloFilePath);
        tmpFile << "Hello, World!";
        tmpFile.close();
        ASSERT_TRUE(std::filesystem::exists(helloFilePath)) << "Failed to create temporary file";
        api::types::v1::PackageInfoV2 packageInfo;
        packageInfo.name = "hello";
        packageInfo.version = "1";
        packageInfo.description = "hello world";
        packageInfo.id = "hello";
        packageInfo.version = "1";
        nlohmann::json json = packageInfo;
        std::ofstream jsonFile(layerDirPath / "info.json");
        jsonFile << json.dump();
        jsonFile.close();
        ASSERT_TRUE(std::filesystem::exists(layerDirPath / "info.json"))
          << layerDirPath << "Failed to create package.json";

        // 创建临时目录，用于存放打包后的layer文件
        layerFileDir = std::make_unique<TempDir>("linglong-test-");
        layerFilePath = layerFileDir->path() / "hello.layer";
        ASSERT_TRUE(layerFileDir->isValid()) << "Failed to create temporary directory";
        package::LayerPackager packager;
        packager.setCompressor("lz4");
        // 生成空文件，测试文件已存在的场景
        std::ofstream emptyFile(layerFilePath);
        emptyFile.close();
        auto layerDir = package::LayerDir(layerDirPath);
        auto ret = packager.pack(layerDir, layerFilePath.string().c_str());
        ASSERT_TRUE(ret.has_value()) << "Failed to pack layer file" << ret.error().message();
        ASSERT_TRUE(std::filesystem::exists(layerFilePath)) << "Failed to pack layer file";
    }

    static void TearDownTestCase()
    {
        std::cout << "Cleanup shared resource" << std::endl;
        layerFileDir.reset();
    }

    void SetUp() override { }

    void TearDown() override { }

    static std::unique_ptr<TempDir> layerFileDir;
    static std::filesystem::path layerFilePath;
};

std::unique_ptr<TempDir> LayerPackagerTest::layerFileDir;
std::filesystem::path LayerPackagerTest::layerFilePath;

TEST_F(LayerPackagerTest, LayerPackagerUnpackFuseOffset)
{
    auto layerFileRet = package::LayerFile::New((layerFilePath).string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message();
    auto layerFile = *layerFileRet;
    package::LayerPackager packager;
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file" << ret.error().message();
    ASSERT_TRUE(std::filesystem::exists(ret->path() / "info.json"))
      << "'info.json' not found in unpack dir" << ret->path();
    auto filesDir = ret->filesDirPath();
    ASSERT_TRUE(std::filesystem::exists(filesDir / "hello"))
      << "'hello' not found in unpack dir" << filesDir;
    // 读取hello文件
    std::ifstream helloFile(filesDir / "hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
}

TEST_F(LayerPackagerTest, LayerPackagerUnpackFuse)
{
    {
        auto ret = utils::Cmd("erofsfuse").exists();
        if (!ret) {
#ifdef GTEST_SKIP
            GTEST_SKIP() << "Skipping this test.";
#else
            return;
#endif
        }
    }
    auto layerFileRet = package::LayerFile::New((layerFilePath).string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message();
    auto layerFile = *layerFileRet;
    MockLayerPackager packager;
    packager.wrapCheckErofsFuseExistsFunc = []() {
        return true;
    };
    packager.wrapIsFileReadableFunc = []([[maybe_unused]] const std::string &path) {
        return false;
    };
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file" << ret.error().message();
    ASSERT_TRUE(std::filesystem::exists(ret->path() / "info.json"))
      << "'info.json' not found in unpack dir" << ret->path();
    auto filesDir = ret->filesDirPath();
    ASSERT_TRUE(std::filesystem::exists(filesDir / "hello"))
      << "'hello' not found in unpack dir" << filesDir;
    // 读取hello文件
    std::ifstream helloFile(filesDir / "hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
}

TEST_F(LayerPackagerTest, LayerPackagerUnpackFsck)
{
    auto layerFileRet = package::LayerFile::New(layerFilePath.string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message();
    auto layerFile = *layerFileRet;
    MockLayerPackager packager;
    packager.wrapCheckErofsFuseExistsFunc = []() {
        return false;
    };
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file" << ret.error().message();
    ASSERT_TRUE(std::filesystem::exists(ret->path() / "info.json"))
      << "'info.json' not found in unpack dir" << ret->path();
    auto filesDir = ret->filesDirPath();
    ASSERT_TRUE(std::filesystem::exists(filesDir / "hello"))
      << "'hello' not found in unpack dir" << filesDir;
}

TEST_F(LayerPackagerTest, InitWorkDir)
{
    TempDir tmpDir("linglong-layer-");
    ASSERT_TRUE(tmpDir.isValid()) << "Failed to create temporary directory";
    MockLayerPackager packager;
    // 测试创建workdir失败时, initWorkDir 应该使用临时目录
    packager.wrapMkdirDirFunc = [](const std::string &path) -> utils::error::Result<void> {
        LINGLONG_TRACE("test workdir not exists");
        if (common::strings::starts_with(path, "/var/tmp")) {
            return LINGLONG_ERR("failed to create work dir");
        }
        return LINGLONG_OK;
    };
    // 测试initWorkDir
    auto ret = packager.initWorkDir();
    ASSERT_TRUE(ret.has_value()) << "Failed to init workdir" << ret.error().message();
    ASSERT_NE(packager.getWorkDir().string(), tmpDir.path() / "not-exists")
      << "workdir should be temporary directory";
}

} // namespace linglong::package
