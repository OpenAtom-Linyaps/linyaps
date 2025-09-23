// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "../mocks/layer_packager_mock.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/package/layer_packager.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/strings.h"

#include <QDir>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace linglong;

namespace linglong::package {

class LayerPackagerTest : public ::testing::Test
{
public:
    static void SetUpTestSuite()
    {
        char tempPath[] = "/var/tmp/linglong-layer-packager-test-SetUpTestSuite-XXXXXX";
        std::filesystem::path layerDirPath = mkdtemp(tempPath);
        ASSERT_FALSE(layerDirPath.empty()) << "Failed to create temporary directory";
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
        char tempPath2[] = "/var/tmp/linglong-test-XXXXXX";
        layerFileDir = mkdtemp(tempPath2);
        layerFilePath = layerFileDir / "hello.layer";
        ASSERT_FALSE(layerFileDir.empty()) << "Failed to create temporary directory";
        package::LayerPackager packager;
        packager.setCompressor("lz4");
        // 生成空文件，测试文件已存在的场景
        std::ofstream emptyFile(layerFilePath);
        emptyFile.close();
        auto layerDir = package::LayerDir(layerDirPath.string().c_str());
        auto ret = packager.pack(layerDir, layerFilePath.string().c_str());
        ASSERT_TRUE(ret.has_value())
          << "Failed to pack layer file" << ret.error().message().toStdString();
        ASSERT_TRUE(std::filesystem::exists(layerFilePath)) << "Failed to pack layer file";
        // 删除layer目录
        std::error_code ec;
        std::filesystem::remove_all(layerDirPath, ec);
        ASSERT_FALSE(ec) << "Failed to remove layer dir" << ec.message();
    }

    static void TearDownTestSuite()
    {
        std::cout << "Cleanup shared resource" << std::endl;
        // 删除layer文件
        std::error_code ec;
        std::filesystem::remove_all(layerFileDir, ec);
        ASSERT_FALSE(ec) << "Failed to remove layer file dir" << ec.message();
    }

    void SetUp() override { }

    void TearDown() override { }

    static std::filesystem::path layerFileDir;
    static std::filesystem::path layerFilePath;
};

std::filesystem::path LayerPackagerTest::layerFileDir;
std::filesystem::path LayerPackagerTest::layerFilePath;

TEST_F(LayerPackagerTest, LayerPackagerUnpackFuseOffset)
{
    auto layerFileRet = package::LayerFile::New((layerFilePath).string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message().toStdString();
    auto layerFile = *layerFileRet;
    package::LayerPackager packager;
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file"
                                 << ret.error().message().toStdString();
    ASSERT_TRUE(std::filesystem::exists(ret->filePath("info.json").toStdString()))
      << "'info.json' not found in unpack dir" << ret->filePath("info.json").toStdString();
    auto filesDir = ret->filesDirPath().toStdString();
    ASSERT_TRUE(std::filesystem::exists(filesDir + "/hello"))
      << "'hello' not found in unpack dir" << filesDir;
    // 读取hello文件
    std::ifstream helloFile(filesDir + "/hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
}

TEST_F(LayerPackagerTest, LayerPackagerUnpackFuse)
{

    auto layerFileRet = package::LayerFile::New((layerFilePath).string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message().toStdString();
    auto layerFile = *layerFileRet;
    MockLayerPackager packager;
    packager.wrapCheckErofsFuseExistsFunc = []() {
        return true;
    };
    packager.wrapIsFileReadableFunc = [](const std::string &path) {
        return false;
    };
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file"
                                 << ret.error().message().toStdString();
    ASSERT_TRUE(std::filesystem::exists(ret->filePath("info.json").toStdString()))
      << "'info.json' not found in unpack dir" << ret->filePath("info.json").toStdString();
    auto filesDir = ret->filesDirPath().toStdString();
    ASSERT_TRUE(std::filesystem::exists(filesDir + "/hello"))
      << "'hello' not found in unpack dir" << filesDir;
    // 读取hello文件
    std::ifstream helloFile(filesDir + "/hello");
    std::stringstream buffer;
    buffer << helloFile.rdbuf();
    ASSERT_EQ(buffer.str(), "Hello, World!") << "Failed to read hello file";
}

TEST_F(LayerPackagerTest, LayerPackagerUnpackFsck)
{
    auto layerFileRet = package::LayerFile::New(layerFilePath.string().c_str());
    ASSERT_TRUE(layerFileRet.has_value())
      << "Failed to create layer file" << layerFileRet.error().message().toStdString();
    auto layerFile = *layerFileRet;
    MockLayerPackager packager;
    packager.wrapCheckErofsFuseExistsFunc = []() {
        return false;
    };
    auto ret = packager.unpack(*layerFile);
    ASSERT_TRUE(ret.has_value()) << "Failed to unpack layer file"
                                 << ret.error().message().toStdString();
    ASSERT_TRUE(std::filesystem::exists(ret->filePath("info.json").toStdString()))
      << "'info.json' not found in unpack dir" << ret->filePath("info.json").toStdString();
    auto filesDir = ret->filesDirPath().toStdString();
    ASSERT_TRUE(std::filesystem::exists(filesDir + "/hello"))
      << "'hello' not found in unpack dir" << filesDir;
}

TEST_F(LayerPackagerTest, InitWorkDir)
{
    char tempPath[] = "/var/tmp/linglong-layer-XXXXXX";
    std::filesystem::path tmpPath = mkdtemp(tempPath);
    MockLayerPackager packager;
    // 测试创建workdir失败时, initWorkDir 应该使用临时目录
    packager.wrapMkdirDirFunc = [](const std::string &path) -> utils::error::Result<void> {
        LINGLONG_TRACE("test workdir not exists");
        if (utils::strings::hasPrefix(path, "/var/tmp")) {
            return LINGLONG_ERR("failed to create work dir");
        }
        return LINGLONG_OK;
    };
    // 测试initWorkDir
    auto ret = packager.initWorkDir();
    ASSERT_TRUE(ret.has_value()) << "Failed to init workdir" << ret.error().message().toStdString();
    ASSERT_NE(packager.getWorkDir().string(), tmpPath / "not-exists")
      << "workdir should be temporary directory";
    // 删除临时目录
    std::error_code ec;
    std::filesystem::remove_all(tmpPath, ec);
    ASSERT_FALSE(ec) << "Failed to remove tmpPath";
}

} // namespace linglong::package