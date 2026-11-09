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

#include <QDir>
#include <QSharedPointer>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace linglong;

namespace linglong::package {

namespace {

// FUSE mounts are not permitted in every environment (for example the GitHub
// Actions runner returns EACCES). Probe with a real mount instead of relying on
// /dev/fuse being present, so that the tests only skip when the capability is
// genuinely unavailable.
bool fuseMountAvailable(const std::filesystem::path &layerFilePath, std::string &reason)
{
    if (!std::filesystem::exists("/dev/fuse")) {
        reason = "fuse device is not available";
        return false;
    }

    if (!utils::Cmd("erofsfuse").exists()) {
        reason = "erofsfuse is not available";
        return false;
    }

    auto layerFileRet = package::LayerFile::New(layerFilePath.string().c_str());
    if (!layerFileRet) {
        reason = "failed to open layer file: " + layerFileRet.error().message();
        return false;
    }

    auto offset = (*layerFileRet)->binaryDataOffset();
    if (!offset) {
        reason = "failed to read layer data offset: " + offset.error().message();
        return false;
    }

    TempDir mountPoint("linglong-fuse-probe-");
    auto mounted = utils::Cmd("erofsfuse")
                     .exec({ "--offset=" + std::to_string(*offset),
                             layerFilePath.string(),
                             mountPoint.path().string() });
    if (!mounted) {
        reason = "fuse mounts are not permitted: " + mounted.error().message();
        return false;
    }

    // erofsfuse daemonizes and exits successfully even when the mount itself
    // fails, so wait for the mounted payload to become visible.
    const auto probeFile = mountPoint.path() / "info.json";
    bool mountReady = std::filesystem::exists(probeFile);
    for (int attempt = 0; !mountReady && attempt < 20; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        mountReady = std::filesystem::exists(probeFile);
    }

    auto unmounted = utils::Cmd("fusermount").exec({ "-z", "-u", mountPoint.path().string() });
    if (!unmounted) {
        utils::Cmd("fusermount3").exec({ "-z", "-u", mountPoint.path().string() });
    }

    if (!mountReady) {
        reason = "fuse mount did not become readable";
        return false;
    }

    return true;
}

// Temporarily sets or unsets an environment variable and restores the previous
// value afterwards, so an override in one test cannot leak into its siblings.
class ScopedEnvVar
{
public:
    ScopedEnvVar(const char *name, const std::string &value)
        : name(name)
    {
        save();
        if (::setenv(name, value.c_str(), 1) != 0) {
            std::cerr << "failed to set environment variable " << name << std::endl;
        }
    }

    explicit ScopedEnvVar(const char *name)
        : name(name)
    {
        save();
        ::unsetenv(name);
    }

    ~ScopedEnvVar() { restore(); }

    ScopedEnvVar(const ScopedEnvVar &) = delete;
    ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

private:
    void save()
    {
        if (const auto *oldValue = std::getenv(name)) {
            previousValue = oldValue;
            hadPreviousValue = true;
        }
    }

    void restore()
    {
        if (hadPreviousValue) {
            ::setenv(name, previousValue.c_str(), 1);
        } else {
            ::unsetenv(name);
        }
    }

    const char *name;
    std::string previousValue;
    bool hadPreviousValue = false;
};

} // namespace

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

        fuseMountSupported = fuseMountAvailable(layerFilePath, fuseSkipReason);
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
    static bool fuseMountSupported;
    static std::string fuseSkipReason;
};

std::unique_ptr<TempDir> LayerPackagerTest::layerFileDir;
std::filesystem::path LayerPackagerTest::layerFilePath;
bool LayerPackagerTest::fuseMountSupported = false;
std::string LayerPackagerTest::fuseSkipReason;

TEST_F(LayerPackagerTest, LayerPackagerUnpackFuseOffset)
{
    if (!fuseMountSupported) {
        GTEST_SKIP() << fuseSkipReason;
    }
    // LayerPackager defaults to /var/tmp, which is not mountable on some
    // runners even though FUSE itself works. Scope the override to the tests
    // that actually mount so InitWorkDir keeps exercising the /var/tmp path.
    TempDir layerWorkRoot("linglong-layer-workdir-");
    ScopedEnvVar tmpDirOverride("LINGLONG_TMPDIR", layerWorkRoot.path().string());
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
    if (!fuseMountSupported) {
        GTEST_SKIP() << fuseSkipReason;
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
    TempDir layerWorkRoot("linglong-layer-workdir-");
    ScopedEnvVar tmpDirOverride("LINGLONG_TMPDIR", layerWorkRoot.path().string());
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
    // Force the default primary path so the mock below rejects the path that
    // initWorkDir actually uses and the temporary-directory fallback runs.
    ScopedEnvVar tmpDirOverride("LINGLONG_TMPDIR");
    MockLayerPackager packager;
    // 测试创建workdir失败时, initWorkDir 应该使用临时目录
    std::vector<std::string> attemptedPaths;
    packager.wrapMkdirDirFunc =
      [&attemptedPaths](const std::string &path) -> utils::error::Result<void> {
        LINGLONG_TRACE("test workdir not exists");
        attemptedPaths.push_back(path);
        if (common::strings::starts_with(path, "/var/tmp")) {
            return LINGLONG_ERR("failed to create work dir");
        }
        return LINGLONG_OK;
    };
    // 测试initWorkDir
    auto ret = packager.initWorkDir();
    ASSERT_TRUE(ret.has_value()) << "Failed to init workdir" << ret.error().message();
    ASSERT_EQ(attemptedPaths.size(), 2U)
      << "initWorkDir should retry under the system temporary directory";
    EXPECT_TRUE(common::strings::starts_with(attemptedPaths.at(0), "/var/tmp"))
      << attemptedPaths.at(0);
    EXPECT_EQ(std::filesystem::path{ attemptedPaths.at(1) }.parent_path(),
              std::filesystem::temp_directory_path());
    EXPECT_EQ(packager.getWorkDir(), std::filesystem::path{ attemptedPaths.at(1) });
}

} // namespace linglong::package
