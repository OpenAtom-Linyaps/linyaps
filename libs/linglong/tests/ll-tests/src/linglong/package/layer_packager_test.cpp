// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
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
#include "linglong/utils/log/log.h"

#include <QDir>
#include <QSharedPointer>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#ifndef _WIN32
#  include <unistd.h>
#endif

using namespace linglong;

namespace linglong::package {

class LayerPackagerTest : public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        auto layerTempDir =
          std::make_unique<TempDir>("linglong-layer-packager-test-SetUpTestSuite-");
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
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }
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
    if (!std::filesystem::exists("/dev/fuse")) {
        GTEST_SKIP() << "fuse device is not available";
    }
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

TEST(LayerPackagerCleanupTest, DestructorCleansUpNestedWorkDir)
{
    std::filesystem::path workDir;
    {
        LayerPackager packager;
        workDir = packager.getWorkDir();
        ASSERT_TRUE(std::filesystem::exists(workDir));

        // Create nested directories and files to verify recursive removal
        auto nestedDir = workDir / "nested" / "sub";
        std::filesystem::create_directories(nestedDir);
        std::ofstream file(nestedDir / "test.txt");
        file << "cleanup test content";
        file.close();
        ASSERT_TRUE(std::filesystem::exists(nestedDir / "test.txt"));
    }
    // Entire workDir and its nested contents should be removed
    ASSERT_FALSE(std::filesystem::exists(workDir));
}

TEST(LayerPackagerCleanupTest, DestructorHandlesAbsentWorkDirWithoutErrorLog)
{
    std::filesystem::path workDir;
    {
        LayerPackager packager;
        workDir = packager.getWorkDir();
        ASSERT_TRUE(std::filesystem::exists(workDir));

        // Pre-remove the workDir before destruction
        std::error_code ec;
        std::filesystem::remove_all(workDir, ec);
        ASSERT_FALSE(ec);
        ASSERT_FALSE(std::filesystem::exists(workDir));

        // Enable console logging and capture stderr
        utils::log::setLogLevel(utils::log::LogLevel::Error);
        utils::log::setLogBackend(utils::log::LogBackend::Console);
        testing::internal::CaptureStderr();

        // Destructor of packager runs when leaving scope
    }
    std::string errOutput = testing::internal::GetCapturedStderr();
    utils::log::setLogBackend(utils::log::LogBackend::None);

    // Verify directory remains absent and no 'failed to remove' was logged
    ASSERT_FALSE(std::filesystem::exists(workDir));
    EXPECT_TRUE(errOutput.find("failed to remove") == std::string::npos)
      << "Expected no 'failed to remove' error log when directory was already absent, got: "
      << errOutput;
}

TEST(LayerPackagerCleanupTest, DestructorHandlesCleanupFailureWithoutCrash)
{
#ifndef _WIN32
    if (::geteuid() == 0) {
        GTEST_SKIP() << "Skipping permission test when running as root";
    }
#endif

    std::filesystem::path workDir;
    std::filesystem::path subDir;
    std::string errOutput;
    {
        LayerPackager packager;
        workDir = packager.getWorkDir();
        ASSERT_TRUE(std::filesystem::exists(workDir));

        // Create a non-empty subdirectory and revoke write permissions so contents cannot be
        // unlinked
        subDir = workDir / "locked_dir";
        std::filesystem::create_directories(subDir);
        std::ofstream file(subDir / "item.txt");
        file << "locked file content";
        file.close();

        std::filesystem::permissions(subDir,
                                     std::filesystem::perms::owner_read
                                       | std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);

        utils::log::setLogLevel(utils::log::LogLevel::Error);
        utils::log::setLogBackend(utils::log::LogBackend::Console);
        testing::internal::CaptureStderr();

        // Destructor of packager runs here when leaving scope.
        // Under old throwing code: throws std::filesystem_error -> triggers std::terminate() ->
        // crash! Under fixed code: safely catches error via std::error_code, logs error, does not
        // terminate.
    }
    errOutput = testing::internal::GetCapturedStderr();
    utils::log::setLogBackend(utils::log::LogBackend::None);

    // Restore permissions so test directory can be safely cleaned up
    std::error_code ec;
    std::filesystem::permissions(subDir,
                                 std::filesystem::perms::all,
                                 std::filesystem::perm_options::replace,
                                 ec);
    std::filesystem::remove_all(workDir, ec);

    // Verify the error was properly reported with failed to remove
    EXPECT_FALSE(errOutput.empty());
    EXPECT_NE(errOutput.find("failed to remove"), std::string::npos)
      << "Expected 'failed to remove' in log output when permission was denied, got: " << errOutput;
}

} // namespace linglong::package
