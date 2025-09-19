/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "../mocks/ostree_repo_mock.h"
#include "linglong/package/reference.h"
#include "linglong/repo/client_factory.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace linglong::repo::test {

namespace fs = std::filesystem;

namespace {

class RepoTest : public ::testing::Test
{
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(RepoTest, exportDir)
{
    // 准备测试环境
    fs::path tempDir = fs::temp_directory_path() / "repo_test";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    EXPECT_FALSE(ec) << "Failed to remove directory: " << ec.message();

    bool created = fs::create_directories(tempDir, ec);
    EXPECT_TRUE(created) << "Failed to create directory";
    EXPECT_FALSE(ec) << "Error creating directory: " << ec.message();
    EXPECT_TRUE(fs::exists(tempDir)) << "Directory not created";

    std::string repoPath = tempDir.string();
    std::string ostreeRepoPath = repoPath + "/repo";
    std::string remoteEndpoint = "https://store-llrepo.deepin.com/repos/";
    std::string remoteRepoName = "repo";
    ClientFactory clientFactory(std::string("http://localhost"));

    // 初始化配置和repo对象
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    QDir repoDir = QString(repoPath.c_str());
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(repoDir, config, clientFactory);

    // 创建测试文件和目录结构，包括XDG标准文件
    fs::path srcDirPath = tempDir / "src";
    created = fs::create_directories(srcDirPath, ec);
    EXPECT_TRUE(created) << "Failed to create source directory";
    EXPECT_FALSE(ec) << "Error creating source directory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath)) << "Source directory not created";

    // 创建普通测试文件
    std::ofstream(srcDirPath / "test1.txt").close();
    std::ofstream(srcDirPath / "test2.txt").close();

    // 创建子目录和文件
    created = fs::create_directories(srcDirPath / "subdir", ec);
    EXPECT_TRUE(created) << "Failed to create subdirectory";
    EXPECT_FALSE(ec) << "Error creating subdirectory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath / "subdir")) << "Subdirectory not created";
    std::ofstream(srcDirPath / "subdir" / "test3.txt").close();

    // 创建XDG标准文件
    created = fs::create_directories(srcDirPath / "share" / "applications" / "test", ec);
    EXPECT_TRUE(created) << "Failed to create applications directory";
    EXPECT_FALSE(ec) << "Error creating applications directory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath / "share" / "applications" / "test"))
      << "Applications directory not created";
    std::ofstream desktopFile(srcDirPath / "share" / "applications" / "test" / "test.desktop");
    desktopFile << "[Desktop Entry]\n"
                << "Name=Test App\n"
                << "Exec=test\n"
                << "Type=Application\n";
    desktopFile.close();

    created = fs::create_directories(srcDirPath / "share" / "dbus-1" / "services", ec);
    EXPECT_TRUE(created) << "Failed to create dbus services directory";
    EXPECT_FALSE(ec) << "Error creating dbus services directory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath / "share" / "dbus-1" / "services"))
      << "Dbus services directory not created";
    std::ofstream dbusServiceFile(srcDirPath / "share" / "dbus-1" / "services"
                                  / "org.test.service");
    dbusServiceFile << "[D-BUS Service]\n"
                    << "Name=org.test\n"
                    << "Exec=/bin/test\n";
    dbusServiceFile.close();

    created = fs::create_directories(srcDirPath / "lib" / "systemd" / "system", ec);
    EXPECT_TRUE(created) << "Failed to create systemd directory";
    EXPECT_FALSE(ec) << "Error creating systemd directory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath / "lib" / "systemd" / "system"))
      << "Systemd directory not created";
    std::ofstream systemdServiceFile(srcDirPath / "lib" / "systemd" / "system" / "test.service");
    systemdServiceFile << "[Unit]\n"
                       << "Description=Test Service\n\n"
                       << "[Service]\n"
                       << "ExecStart=/bin/test\n";
    systemdServiceFile.close();

    // 测试exportDir功能
    fs::path destDirPath = tempDir / "entries";
    {
        // 测试目标目录已存在同名文件的情况
        created = fs::create_directories(destDirPath / "share" / "applications", ec);
        EXPECT_TRUE(created) << "Failed to create destination applications directory";
        EXPECT_FALSE(ec) << "Error creating destination applications directory: " << ec.message();
        EXPECT_TRUE(fs::exists(destDirPath / "share" / "applications"))
          << "Destination applications directory not created";
        std::ofstream(destDirPath / "share" / "applications" / "test").close();
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();

        // 非标准路径会被忽略
        EXPECT_TRUE(fs::exists(destDirPath / "test1.txt"));
        EXPECT_TRUE(fs::exists(destDirPath / "test2.txt"));
        EXPECT_TRUE(fs::exists(destDirPath / "subdir" / "test3.txt"));

        // 验证XDG标准文件
        EXPECT_TRUE(fs::exists(destDirPath / "share" / "applications" / "test" / "test.desktop"));
        EXPECT_TRUE(
          fs::exists(srcDirPath / "share/applications/test/test.desktop.linyaps.original"));
        EXPECT_TRUE(
          !fs::exists(destDirPath / "share/applications/test/test.desktop.linyaps.original"));
        EXPECT_TRUE(fs::exists(destDirPath / "share" / "dbus-1" / "services" / "org.test.service"));
        EXPECT_TRUE(fs::exists(destDirPath / "lib" / "systemd" / "system" / "test.service"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    }
    ostreeRepo->warpGetOverlayShareDirFunc = [destDirPath]() {
        return QDir(QString((destDirPath / "apps/share").string().c_str()));
    };
    // 如果defaultShareDir已存在desktop, 则优先导出到defaultShareDir目录
    {
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
        EXPECT_TRUE(fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(!fs::exists(destDirPath / "app/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    }
    // 如果defaultShareDir不存在desktop, 则导出到overlayShareDir目录
    fs::remove_all(destDirPath);
    {
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_TRUE(!fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::exists(destDirPath / "apps/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
    }
    // 如果两个目录都有desktop，则导出到两个目录
    {
        std::ofstream(destDirPath / "share/applications/test/test.desktop").close();
        EXPECT_TRUE(!fs::is_symlink(destDirPath / "share/applications/test/test.desktop"));
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
        EXPECT_TRUE(fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::exists(destDirPath / "apps/share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::is_symlink(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::is_symlink(destDirPath / "apps/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value())
          << "exportDir failed: " << result.error().message().toStdString();
    }

    // 测试空目录导出
    fs::path emptyDirPath = tempDir / "empty";
    created = fs::create_directories(emptyDirPath, ec);
    EXPECT_TRUE(created) << "Failed to create empty directory";
    EXPECT_FALSE(ec) << "Error creating empty directory: " << ec.message();
    EXPECT_TRUE(fs::exists(emptyDirPath)) << "Empty directory not created";
    fs::path emptyDestPath = tempDir / "empty_dest";
    auto result = ostreeRepo->exportDir("appID", emptyDirPath.string(), emptyDestPath.string(), 10);
    EXPECT_TRUE(result.has_value())
      << "exportDir failed: " << result.error().message().toStdString();
    EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    EXPECT_TRUE(fs::exists(emptyDestPath));
}

} // namespace
} // namespace linglong::repo::test
