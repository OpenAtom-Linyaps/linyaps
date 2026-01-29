/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
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

    // 初始化配置和repo对象
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    QDir repoDir = QString(repoPath.c_str());
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(repoDir, config);

    // 创建测试文件和目录结构，包括XDG标准文件
    fs::path srcDirPath = tempDir / "src";
    created = fs::create_directories(srcDirPath, ec);
    EXPECT_TRUE(created) << "Failed to create source directory";
    EXPECT_FALSE(ec) << "Error creating source directory: " << ec.message();
    EXPECT_TRUE(fs::exists(srcDirPath)) << "Source directory not created";

    // 创建普通测试文件
    std::ofstream(srcDirPath / "test1.txt").close();
    std::ofstream(srcDirPath / "test2.txt") << "test2.txt";

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
    ostreeRepo->wrapGetOverlayShareDirFunc = [destDirPath]() {
        return QDir(QString((destDirPath / "share").string().c_str()));
    };
    {
        // 测试目标目录已存在同名文件的情况
        fs::create_directories(destDirPath, ec);
        fs::create_symlink("noexist", destDirPath / "test2.txt");
        created = fs::create_directories(destDirPath / "share" / "applications", ec);
        EXPECT_TRUE(created) << "Failed to create destination applications directory";
        EXPECT_FALSE(ec) << "Error creating destination applications directory: " << ec.message();
        EXPECT_TRUE(fs::exists(destDirPath / "share" / "applications"))
          << "Destination applications directory not created";
        std::ofstream(destDirPath / "share" / "applications" / "test").close();
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        auto status = fs::status(destDirPath / "share" / "applications" / "test", ec);
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
        EXPECT_EQ(status.type(), fs::file_type::directory);

        // 非标准路径会被忽略
        EXPECT_TRUE(fs::exists(destDirPath / "test1.txt"));
        EXPECT_TRUE(fs::exists(destDirPath / "test2.txt"));
        std::ifstream ifs(destDirPath / "test2.txt");
        std::string content((std::istreambuf_iterator<char>(ifs)),
                            (std::istreambuf_iterator<char>()));
        EXPECT_EQ(content, "test2.txt");
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
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    }
    ostreeRepo->wrapGetOverlayShareDirFunc = [destDirPath]() {
        return QDir(QString((destDirPath / "apps/share").string().c_str()));
    };
    // 如果defaultShareDir已存在desktop, 则优先导出到defaultShareDir目录
    {
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
        EXPECT_TRUE(fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(!fs::exists(destDirPath / "app/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    }
    // 如果defaultShareDir不存在desktop, 则导出到overlayShareDir目录
    fs::remove_all(destDirPath);
    {
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        EXPECT_TRUE(!fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::exists(destDirPath / "apps/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
    }
    // 如果两个目录都有desktop，则导出到两个目录
    {
        std::ofstream(destDirPath / "share/applications/test/test.desktop").close();
        EXPECT_TRUE(!fs::is_symlink(destDirPath / "share/applications/test/test.desktop"));
        auto result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
        EXPECT_TRUE(fs::exists(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::exists(destDirPath / "apps/share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::is_symlink(destDirPath / "share/applications/test/test.desktop"));
        EXPECT_TRUE(fs::is_symlink(destDirPath / "apps/share/applications/test/test.desktop"));
        // 测试重复导出
        result = ostreeRepo->exportDir("appID", srcDirPath.string(), destDirPath.string(), 10);
        EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
    }

    // 测试空目录导出
    fs::path emptyDirPath = tempDir / "empty";
    created = fs::create_directories(emptyDirPath, ec);
    EXPECT_TRUE(created) << "Failed to create empty directory";
    EXPECT_FALSE(ec) << "Error creating empty directory: " << ec.message();
    EXPECT_TRUE(fs::exists(emptyDirPath)) << "Empty directory not created";
    fs::path emptyDestPath = tempDir / "empty_dest";
    auto result = ostreeRepo->exportDir("appID", emptyDirPath.string(), emptyDestPath.string(), 10);
    EXPECT_TRUE(result.has_value()) << "exportDir failed: " << result.error().message();
    EXPECT_FALSE(ec) << "Unexpected error code: " << ec.message();
    EXPECT_TRUE(fs::exists(emptyDestPath));
}

} // namespace

namespace {

using ::testing::_;
using ::testing::Return;

struct AppData
{
    const char *app_id;
    const char *arch;
    const char *base;
    const char *channel;
    const char *description;
    const char *id;
    const char *kind;
    const char *module;
    const char *name;
    const char *repo_name;
    const char *runtime;
    long size;
    const char *uab_url;
    const char *version;
};

auto create_response_from_data(const std::vector<AppData> &app_data_list)
{
    list_t *list = list_createList();
    for (const auto &data : app_data_list) {
        auto *item = request_register_struct_create(
          strdup(data.app_id ? data.app_id : "com.example.app"),
          strdup(data.arch ? data.arch : "x86_64"),
          strdup(data.base ? data.base : "base"),
          strdup(data.channel ? data.channel : "main"),
          strdup(data.description ? data.description : "description"),
          strdup(data.id ? data.id : "id"),
          strdup(data.kind ? data.kind : "app"),
          strdup(data.module ? data.module : "binary"),
          strdup(data.name ? data.name : "name"),
          strdup(data.repo_name ? data.repo_name : "stable"),
          strdup(data.runtime ? data.runtime : "runtime"),
          data.size,
          strdup(data.uab_url ? data.uab_url : "localhost"),
          strdup(data.version ? data.version : "1.0.0"));
        list_addElement(list, item);
    }
    return fuzzy_search_app_200_response_create(200, list, nullptr, nullptr);
}

class OSTreeRepoMock : public repo::OSTreeRepo
{
public:
    OSTreeRepoMock(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(std::unique_ptr<ClientAPIWrapper>,
                createClientV2,
                (const std::string &url),
                (override, const));
};

class MockClientAPIWrapper : public ClientAPIWrapper
{
public:
    MockClientAPIWrapper(apiClient_t *client)
        : ClientAPIWrapper(client)
    {
    }

    MOCK_METHOD((std::unique_ptr<fuzzy_search_app_200_response_t,
                                 decltype(&fuzzy_search_app_200_response_free)>),
                fuzzySearch,
                (request_fuzzy_search_req_t * req),
                (override));
};

TEST(OSTreeRepoTest, searchRemote_Search)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };
    auto client = apiClient_create_with_base_path(repoConfig.url.c_str(), nullptr, nullptr);
    auto clientAPI = new MockClientAPIWrapper(client);

    std::vector<AppData> test_data = { { .app_id = "com.example.cpp", .version = "1.0.0" } };
    auto resp = create_response_from_data(test_data);

    EXPECT_CALL(*clientAPI, fuzzySearch(_))
      .WillOnce(Return(std::unique_ptr<fuzzy_search_app_200_response_t,
                                       decltype(&fuzzy_search_app_200_response_free)>(
        resp,
        &fuzzy_search_app_200_response_free)));
    EXPECT_CALL(mockRepo, createClientV2(repoConfig.url))
      .WillOnce(Return(std::unique_ptr<ClientAPIWrapper>(clientAPI)));

    auto result = repo.searchRemote(*fuzzyRef, repoConfig);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ((*result)[0].id, "com.example.cpp");
    EXPECT_EQ((*result)[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, searchRemote_MatchVersion)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app/1.0");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };
    auto client = apiClient_create_with_base_path(repoConfig.url.c_str(), nullptr, nullptr);
    auto clientAPI = new MockClientAPIWrapper(client);

    std::vector<AppData> test_data = { { .app_id = "com.example.app", .version = "1.0.0" },
                                       { .app_id = "com.example.app", .version = "2.0.0" },
                                       { .app_id = "com.example.app", .version = "2.1.0" } };
    auto resp = create_response_from_data(test_data);

    EXPECT_CALL(*clientAPI, fuzzySearch(_))
      .WillOnce(Return(std::unique_ptr<fuzzy_search_app_200_response_t,
                                       decltype(&fuzzy_search_app_200_response_free)>(
        resp,
        &fuzzy_search_app_200_response_free)));
    EXPECT_CALL(mockRepo, createClientV2(repoConfig.url))
      .WillOnce(Return(std::unique_ptr<ClientAPIWrapper>(clientAPI)));

    auto result = repo.searchRemote(*fuzzyRef, repoConfig, true);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ((*result)[0].id, "com.example.app");
    EXPECT_EQ((*result)[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, searchRemote_MatchId)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("example");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };
    auto client = apiClient_create_with_base_path(repoConfig.url.c_str(), nullptr, nullptr);
    auto clientAPI = new MockClientAPIWrapper(client);

    std::vector<AppData> test_data = { { .app_id = "com.example", .version = "1.0.0" },
                                       { .app_id = "example.app2", .version = "2.0.0" },
                                       { .app_id = "com.example.app3", .version = "1.0.0" },
                                       { .app_id = "example", .version = "1.0.0" } };
    auto resp = create_response_from_data(test_data);

    EXPECT_CALL(*clientAPI, fuzzySearch(_))
      .WillOnce(Return(std::unique_ptr<fuzzy_search_app_200_response_t,
                                       decltype(&fuzzy_search_app_200_response_free)>(
        resp,
        &fuzzy_search_app_200_response_free)));
    EXPECT_CALL(mockRepo, createClientV2(repoConfig.url))
      .WillOnce(Return(std::unique_ptr<ClientAPIWrapper>(clientAPI)));

    auto result = repo.searchRemote(*fuzzyRef, repoConfig, true);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_EQ((*result)[0].id, "example");
    EXPECT_EQ((*result)[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, searchRemote_Empty)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.nonexistent.app");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };
    auto client = apiClient_create_with_base_path(repoConfig.url.c_str(), nullptr, nullptr);
    auto clientAPI = new MockClientAPIWrapper(client);

    std::vector<AppData> test_data = {};
    auto resp = create_response_from_data(test_data);

    EXPECT_CALL(*clientAPI, fuzzySearch(_))
      .WillOnce(Return(std::unique_ptr<fuzzy_search_app_200_response_t,
                                       decltype(&fuzzy_search_app_200_response_free)>(
        resp,
        &fuzzy_search_app_200_response_free)));
    EXPECT_CALL(mockRepo, createClientV2(repoConfig.url))
      .WillOnce(Return(std::unique_ptr<ClientAPIWrapper>(clientAPI)));

    auto result = repo.searchRemote(*fuzzyRef, repoConfig, true);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(OSTreeRepoTest, searchRemote_RemoteError)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };
    auto client = apiClient_create_with_base_path(repoConfig.url.c_str(), nullptr, nullptr);
    auto clientAPI = new MockClientAPIWrapper(client);

    EXPECT_CALL(*clientAPI, fuzzySearch(_))
      .WillOnce(Return(std::unique_ptr<fuzzy_search_app_200_response_t,
                                       decltype(&fuzzy_search_app_200_response_free)>(
        nullptr,
        &fuzzy_search_app_200_response_free)));
    EXPECT_CALL(mockRepo, createClientV2(repoConfig.url))
      .WillOnce(Return(std::unique_ptr<ClientAPIWrapper>(clientAPI)));

    auto result = repo.searchRemote(*fuzzyRef, repoConfig, true);

    EXPECT_FALSE(result.has_value());
}

namespace {

class OSTreeRepoMock : public repo::OSTreeRepo
{
public:
    OSTreeRepoMock(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>,
                searchRemote,
                (const package::FuzzyReference &fuzzyRef,
                 const api::types::v1::Repo &repo,
                 bool semanticMatching),
                (override, const, noexcept));

    MOCK_METHOD(std::vector<std::vector<api::types::v1::Repo>>,
                getPriorityGroupedRepos,
                (),
                (override, const, noexcept));
};

TEST(OSTreeRepoTest, matchRemoteByPriority_SpecifiedRepo)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");
    auto repoConfig = api::types::v1::Repo{ .name = "test", .url = "http://localhost:8080" };

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{ .id = "com.example.app", .version = "1.0.0" },
      }));

    auto result = repo.matchRemoteByPriority(*fuzzyRef, repoConfig);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
    const auto &repoPackages = result->getRepoPackages();
    EXPECT_EQ(repoPackages.size(), 1);
    EXPECT_EQ(repoPackages.front().first.name, "test");
    EXPECT_EQ(repoPackages.front().second[0].id, "com.example.app");
    EXPECT_EQ(repoPackages.front().second[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, matchRemoteByPriority_NoRepoSpecified_Success)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo1", .priority = 2, .url = "http://localhost:8081" },
        },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo2", .priority = 1, .url = "http://localhost:8081" },
          api::types::v1::Repo{ .name = "repo3", .priority = 1, .url = "http://localhost:8082" } },
      }));

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{ .id = "com.example.app", .version = "1.0.0" },
      }));

    auto result = repo.matchRemoteByPriority(*fuzzyRef);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
    const auto &repoPackages = result->getRepoPackages();
    EXPECT_EQ(repoPackages.size(), 1);
    EXPECT_EQ(repoPackages.front().first.name, "repo1");
    EXPECT_EQ(repoPackages.front().second[0].id, "com.example.app");
    EXPECT_EQ(repoPackages.front().second[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, matchRemoteByPriority_FallbackToLowerPriority)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo1", .priority = 2, .url = "http://localhost:8081" },
        },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo2", .priority = 1, .url = "http://localhost:8081" },
          api::types::v1::Repo{ .name = "repo3", .priority = 1, .url = "http://localhost:8082" } },
      }));

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{ .id = "com.example.app", .version = "1.0.0" },
      }))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}));

    auto result = repo.matchRemoteByPriority(*fuzzyRef);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());
    const auto &repoPackages = result->getRepoPackages();
    EXPECT_EQ(repoPackages.size(), 1);
    EXPECT_EQ(repoPackages.front().first.name, "repo2");
    EXPECT_EQ(repoPackages.front().second[0].id, "com.example.app");
    EXPECT_EQ(repoPackages.front().second[0].version, "1.0.0");
}

TEST(OSTreeRepoTest, matchRemoteByPriority_AllReposEmpty)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo1", .priority = 2, .url = "http://localhost:8081" },
        },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo2", .priority = 1, .url = "http://localhost:8081" },
          api::types::v1::Repo{ .name = "repo3", .priority = 1, .url = "http://localhost:8082" } },
      }));

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}));

    auto result = repo.matchRemoteByPriority(*fuzzyRef);

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(OSTreeRepoTest, matchRemoteByPriority_AllReposError)
{
    LINGLONG_TRACE("matchRemoteByPriority_AllReposError");
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo1", .priority = 2, .url = "http://localhost:8081" },
        },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo2", .priority = 1, .url = "http://localhost:8081" },
          api::types::v1::Repo{ .name = "repo3", .priority = 1, .url = "http://localhost:8082" } },
      }));

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(LINGLONG_ERR("error")))
      .WillOnce(Return(LINGLONG_ERR("error")))
      .WillOnce(Return(LINGLONG_ERR("error")));

    auto result = repo.matchRemoteByPriority(*fuzzyRef);

    EXPECT_FALSE(result.has_value());
}

TEST(OSTreeRepoTest, matchRemoteByPriority_NoReposConfigured)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{}));

    auto result = repo.matchRemoteByPriority(*fuzzyRef);

    EXPECT_FALSE(result.has_value());
}

TEST(OSTreeRepoTest, matchRemoteByPriority_UseHighestPriority)
{
    TempDir tempDir;
    OSTreeRepoMock mockRepo(tempDir.path());
    repo::OSTreeRepo &repo = mockRepo;

    auto fuzzyRef = package::FuzzyReference::parse("com.example.app");

    EXPECT_CALL(mockRepo, getPriorityGroupedRepos())
      .WillOnce(Return(std::vector<std::vector<api::types::v1::Repo>>{
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo1", .priority = 3, .url = "http://localhost:8081" },
        },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo2", .priority = 2, .url = "http://localhost:8081" },
          api::types::v1::Repo{ .name = "repo3", .priority = 2, .url = "http://localhost:8082" } },
        std::vector<api::types::v1::Repo>{
          api::types::v1::Repo{ .name = "repo4", .priority = 1, .url = "http://localhost:8081" },
        },
      }));

    EXPECT_CALL(mockRepo, searchRemote(_, _, true))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{ .id = "com.example.app", .version = "2.0.0" },
      }))
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        api::types::v1::PackageInfoV2{ .id = "com.example.app", .version = "3.0.0" },
      }));

    auto result = repo.matchRemoteByPriority(*fuzzyRef, std::nullopt);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty());

    const auto &repoPackages = result->getRepoPackages();
    EXPECT_EQ(repoPackages.size(), 2);
    EXPECT_EQ(repoPackages.front().first.name, "repo2");
    EXPECT_EQ(repoPackages.front().second[0].id, "com.example.app");
    EXPECT_EQ(repoPackages.front().second[0].version, "2.0.0");
    EXPECT_EQ(repoPackages.back().first.name, "repo3");
    EXPECT_EQ(repoPackages.back().second[0].id, "com.example.app");
    EXPECT_EQ(repoPackages.back().second[0].version, "3.0.0");
}

} // namespace

} // namespace

} // namespace linglong::repo::test
