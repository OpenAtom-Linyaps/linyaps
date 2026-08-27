/*
 * SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/scoped_umask.h"
#include "../../common/tempdir.h"
#include "../mocks/ostree_repo_mock.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/common/constants.h"
#include "linglong/package/reference.h"
#include "linglong/repo/client_factory.h"
#include "linglong/repo/config.h"
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

api::types::v1::RepoConfigV2 createRepoConfig()
{
    return api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .name = "stable",
                                         .priority = 0,
                                         .url = "https://example.com/repo" } },
        .version = 2,
    };
}

api::types::v1::RepoConfigV2 createRepoConfig(std::string defaultRepo,
                                              std::string repoName,
                                              std::string repoUrl)
{
    return api::types::v1::RepoConfigV2{
        .defaultRepo = std::move(defaultRepo),
        .repos = { api::types::v1::Repo{ .name = std::move(repoName),
                                         .priority = 0,
                                         .url = std::move(repoUrl) } },
        .version = 2,
    };
}

class RepoTest : public ::testing::Test
{
protected:
    void SetUp() override { }

    void TearDown() override { }
};

TEST_F(RepoTest, resolveDesktopFileExportPathUsesOverlayWhenPresent)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    const auto defaultDesktopPath = tempDir.path() / "entries/share/applications/org.test.desktop";
    const auto overlayDesktopPath =
      tempDir.path() / "entries/apps/share/applications/org.test.desktop";

    std::filesystem::create_directories(defaultDesktopPath.parent_path());
    std::filesystem::create_directories(overlayDesktopPath.parent_path());
    std::ofstream(overlayDesktopPath) << "overlay";

    ostreeRepo->wrapGetOverlayShareDirFunc = [&overlayDesktopPath]() {
        return overlayDesktopPath.parent_path().parent_path();
    };

    EXPECT_EQ(ostreeRepo->resolveDesktopFileExportPath("applications/org.test.desktop"),
              overlayDesktopPath);
}

TEST_F(RepoTest, resolveEntryExportPathMapsLegacySystemdUserPath)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    EXPECT_EQ(ostreeRepo->resolveEntryExportPath("share/systemd/user/test.service", false),
              tempDir.path() / "entries/lib/systemd/user/test.service");
}

TEST_F(RepoTest, resolveEntryExportPathSkipsLegacySystemdUserWhenLibPathPreferred)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    EXPECT_TRUE(
      ostreeRepo->resolveEntryExportPath("share/systemd/user/test.service", true).empty());
}

TEST_F(RepoTest, createPersistsConfigAndBootstrapsRepoArtifacts)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    auto repoRoot = tempDir.path() / "repo-root";
    ASSERT_TRUE(fs::create_directories(repoRoot));

    auto config = createRepoConfig();
    ScopedUmask scopedUmask{ 0022 };
    auto repo = OSTreeRepo::create(repoRoot, config);
    ASSERT_TRUE(repo.has_value()) << repo.error().message();

    EXPECT_TRUE(fs::exists(repoRoot / "config.yaml"));
    EXPECT_TRUE(fs::exists(repoRoot / "repo"));
    EXPECT_TRUE(fs::exists(repoRoot / "states.json"));
    EXPECT_EQ(fs::status(repoRoot / "config.yaml").permissions() & fs::perms::mask,
              common::shared_file_permissions);
    EXPECT_EQ(fs::status(repoRoot / "repo").permissions() & fs::perms::mask,
              common::shared_directory_permissions);
    EXPECT_EQ(fs::status(repoRoot / "states.json").permissions() & fs::perms::mask,
              common::shared_file_permissions);
    EXPECT_EQ(fs::status(repoRoot / ".version").permissions() & fs::perms::mask,
              common::shared_file_permissions);

    auto entriesResult = repo->get()->fixExportAllEntries();
    ASSERT_TRUE(entriesResult.has_value()) << entriesResult.error().message();
    EXPECT_EQ(fs::status(repoRoot / "entries").permissions() & fs::perms::mask,
              common::shared_directory_permissions);
    EXPECT_EQ(fs::status(repoRoot / "entries/.version").permissions() & fs::perms::mask,
              common::shared_file_permissions);

    auto loaded = OSTreeRepo::loadFromPath(repoRoot);
    EXPECT_TRUE(loaded.has_value()) << loaded.error().message();
}

TEST_F(RepoTest, exportLayerSignDataExportsWhitelistedPathForEveryLayerKindAndModule)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);
    ostreeRepo->wrapShouldExportSignDataFunc = []() -> utils::error::Result<bool> {
        return true;
    };

    const std::array<std::pair<std::string, std::string>, 3> layerTypes{
        std::pair{ "base", "develop" },
        std::pair{ "runtime", "runtime" },
        std::pair{ "custom", "custom-module" },
    };

    ScopedUmask scopedUmask{ 0022 };
    for (std::size_t i = 0; i < layerTypes.size(); ++i) {
        const auto commit = "commit-" + std::to_string(i);
        const auto source = tempDir.path() / "layers" / commit / "entries";
        fs::create_directories(source / "share/deepin-elf-verify/.elfsign");
        fs::create_directories(source / "share/applications");
        std::ofstream(source / "share/deepin-elf-verify/.elfsign/signature") << commit;
        std::ofstream(source / "share/applications/not-exported.desktop") << "desktop";

        api::types::v1::RepositoryCacheLayersItem item{
            .commit = commit,
            .info =
              api::types::v1::PackageInfoV2{
                .id = "org.test." + std::to_string(i),
                .kind = layerTypes[i].first,
                .packageInfoV2Module = layerTypes[i].second,
              },
        };

        auto result = ostreeRepo->exportLayerSignData(tempDir.path() / "entries", item);
        ASSERT_TRUE(result.has_value()) << result.error().message();
        EXPECT_TRUE(fs::is_symlink(tempDir.path() / "entries/share/deepin-elf-verify" / commit
                                   / ".elfsign/signature"));
    }

    EXPECT_FALSE(fs::exists(tempDir.path() / "entries/share/applications"));
    for (const auto &entry : fs::recursive_directory_iterator(tempDir.path() / "entries")) {
        if (entry.is_directory()) {
            EXPECT_EQ(entry.status().permissions() & fs::perms::mask,
                      common::shared_directory_permissions)
              << entry.path();
        }
    }
}

TEST_F(RepoTest, exportLayerSignDataSkipsPathNotInWhitelist)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);
    ostreeRepo->wrapShouldExportSignDataFunc = []() -> utils::error::Result<bool> {
        return false;
    };

    const std::string commit = "not-whitelisted";
    const auto source = tempDir.path() / "layers" / commit / "entries";
    fs::create_directories(source / "share/deepin-elf-verify/.elfsign");
    std::ofstream(source / "share/deepin-elf-verify/.elfsign/signature") << commit;

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = commit,
        .info = api::types::v1::PackageInfoV2{ .id = "org.test.not-whitelisted",
                                               .kind = "runtime",
                                               .packageInfoV2Module = "binary" },
    };

    auto result = ostreeRepo->exportLayerSignData(tempDir.path() / "entries", item);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(fs::exists(tempDir.path() / "entries/share/deepin-elf-verify" / commit));
}

TEST_F(RepoTest, exportLayerSignDataPropagatesWhitelistError)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);
    ostreeRepo->wrapShouldExportSignDataFunc = []() -> utils::error::Result<bool> {
        LINGLONG_TRACE("mock invalid export config");
        return LINGLONG_ERR("invalid export config");
    };

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = "invalid-config",
        .info = api::types::v1::PackageInfoV2{ .id = "org.test.invalid-config",
                                               .kind = "runtime",
                                               .packageInfoV2Module = "binary" },
    };

    auto result = ostreeRepo->exportLayerSignData(tempDir.path() / "entries", item);
    EXPECT_FALSE(result.has_value());
}

TEST_F(RepoTest, unexportAppEntriesRemovesSelectedModulesAndPreservesLayerSignData)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);
    ostreeRepo->wrapShouldExportSignDataFunc = []() -> utils::error::Result<bool> {
        return true;
    };

    const auto entriesDir = tempDir.path() / "entries";
    const std::array<std::string, 2> commits{ "binary-commit", "develop-commit" };
    std::vector<std::filesystem::path> layerDirs;
    std::vector<api::types::v1::RepositoryCacheLayersItem> items;

    for (const auto &commit : commits) {
        auto layerDir = tempDir.path() / "layers" / commit;
        auto source = layerDir / "entries";
        fs::create_directories(source / "share/applications");
        fs::create_directories(source / "share/deepin-elf-verify/.elfsign");
        std::ofstream(source / "share/applications" / (commit + ".desktop"))
          << "[Desktop Entry]\nType=Application\nName=" << commit << "\nExec=true\n";
        std::ofstream(source / "share/deepin-elf-verify/.elfsign/signature") << commit;

        api::types::v1::RepositoryCacheLayersItem item{
            .commit = commit,
            .info = api::types::v1::PackageInfoV2{ .id = "org.test.app",
                                                   .kind = "app",
                                                   .packageInfoV2Module = commit },
        };
        auto exported = ostreeRepo->exportLayerSignData(entriesDir, item);
        ASSERT_TRUE(exported.has_value()) << exported.error().message();

        fs::create_directories(entriesDir / "share/applications");
        fs::create_symlink(source / "share/applications" / (commit + ".desktop"),
                           entriesDir / "share/applications" / (commit + ".desktop"));
        layerDirs.emplace_back(std::move(layerDir));
        items.emplace_back(std::move(item));
    }

    auto appEntriesUnexported = ostreeRepo->unexportAppEntries(entriesDir, { layerDirs.front() });
    ASSERT_TRUE(appEntriesUnexported.has_value()) << appEntriesUnexported.error().message();
    EXPECT_FALSE(
      fs::exists(entriesDir / "share/applications" / (items.front().commit + ".desktop")));
    EXPECT_TRUE(fs::exists(entriesDir / "share/applications" / (items.back().commit + ".desktop")));
    for (const auto &item : items) {
        EXPECT_TRUE(
          fs::exists(entriesDir / "share/deepin-elf-verify" / item.commit / ".elfsign/signature"));
    }

    appEntriesUnexported = ostreeRepo->unexportAppEntries(entriesDir, { layerDirs.back() });
    ASSERT_TRUE(appEntriesUnexported.has_value()) << appEntriesUnexported.error().message();
    EXPECT_FALSE(
      fs::exists(entriesDir / "share/applications" / (items.back().commit + ".desktop")));

    for (const auto &item : items) {
        auto signDataUnexported = ostreeRepo->unexportLayerSignData(entriesDir, item);
        ASSERT_TRUE(signDataUnexported.has_value()) << signDataUnexported.error().message();
        EXPECT_FALSE(fs::exists(entriesDir / "share/deepin-elf-verify" / item.commit));
    }
}

TEST_F(RepoTest, exportAppEntriesDoesNotExportElfVerificationDataAgain)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    const std::string commit = "app-commit";
    const auto source = tempDir.path() / "layers" / commit / "entries";
    fs::create_directories(source / "share/deepin-elf-verify/.elfsign");
    std::ofstream(source / "share/deepin-elf-verify/.elfsign/signature") << commit;

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = commit,
        .info =
          api::types::v1::PackageInfoV2{
            .id = "org.test.app",
            .kind = "app",
            .packageInfoV2Module = "binary",
          },
    };

    std::ignore = ostreeRepo->exportAppEntries(tempDir.path() / "entries", item);

    EXPECT_FALSE(fs::exists(tempDir.path() / "entries/share/deepin-elf-verify"));
}

TEST_F(RepoTest, moduleMergesUseBinaryInfo)
{
    TempDir tempDir;
    TempDir developDir;
    TempDir binaryDir;
    ASSERT_TRUE(tempDir.isValid());
    ASSERT_TRUE(developDir.isValid());
    ASSERT_TRUE(binaryDir.isValid());

    auto repoRoot = tempDir.path() / "repo-root";
    ASSERT_TRUE(fs::create_directories(repoRoot));
    auto repo = OSTreeRepo::create(repoRoot, createRepoConfig());
    ASSERT_TRUE(repo.has_value()) << repo.error().message();

    auto makeInfo = [](std::string module) {
        return api::types::v1::PackageInfoV2{
            .arch = std::vector<std::string>{ "x86_64" },
            .channel = "main",
            .id = "org.test.merge",
            .kind = "app",
            .packageInfoV2Module = std::move(module),
            .version = "1.0.0",
        };
    };
    const auto developInfo = makeInfo("develop");
    const auto binaryInfo = makeInfo("binary");

    std::ofstream(developDir.path() / "info.json") << nlohmann::json(developInfo).dump();
    std::ofstream(binaryDir.path() / "info.json") << nlohmann::json(binaryInfo).dump();

    auto importedDevelop = repo->get()->importLayerDir(package::LayerDir{ developDir.path() });
    ASSERT_TRUE(importedDevelop.has_value()) << importedDevelop.error().message();
    EXPECT_FALSE(importedDevelop->commit.empty());
    EXPECT_EQ(importedDevelop->info.packageInfoV2Module, "develop");
    EXPECT_EQ(importedDevelop->repo, "local");
    auto importedBinary = repo->get()->importLayerDir(package::LayerDir{ binaryDir.path() });
    ASSERT_TRUE(importedBinary.has_value()) << importedBinary.error().message();
    EXPECT_FALSE(importedBinary->commit.empty());
    EXPECT_EQ(importedBinary->info.packageInfoV2Module, "binary");
    EXPECT_EQ(importedBinary->repo, "local");

    auto ref = package::Reference::fromPackageInfo(binaryInfo);
    ASSERT_TRUE(ref.has_value()) << ref.error().message();
    ASSERT_FALSE(fs::exists(repoRoot / "merged"));
    fs::path temporaryMergedPath;
    {
        auto merged =
          repo->get()->createTempMergedModuleDir(*ref,
                                                 std::vector<std::string>{ "develop", "binary" });
        ASSERT_TRUE(merged.has_value()) << merged.error().message();
        temporaryMergedPath = merged->path();
        EXPECT_TRUE(fs::exists(temporaryMergedPath));

        auto anotherMerged =
          repo->get()->createTempMergedModuleDir(*ref,
                                                 std::vector<std::string>{ "develop", "binary" });
        ASSERT_TRUE(anotherMerged.has_value()) << anotherMerged.error().message();
        EXPECT_NE(anotherMerged->path(), temporaryMergedPath);
        EXPECT_TRUE(fs::exists(anotherMerged->path()));

        auto mergedInfo = merged->layerDir().info();
        ASSERT_TRUE(mergedInfo.has_value()) << mergedInfo.error().message();
        EXPECT_EQ(mergedInfo->packageInfoV2Module, "binary");
    }
    EXPECT_FALSE(fs::exists(temporaryMergedPath));

    auto mergeResult = repo->get()->mergeModules();
    ASSERT_TRUE(mergeResult.has_value()) << mergeResult.error().message();
    auto persistentMerged = repo->get()->getMergedModuleDir(*ref, false);
    ASSERT_TRUE(persistentMerged.has_value()) << persistentMerged.error().message();
    auto persistentMergedInfo = persistentMerged->info();
    ASSERT_TRUE(persistentMergedInfo.has_value()) << persistentMergedInfo.error().message();
    EXPECT_EQ(persistentMergedInfo->packageInfoV2Module, "binary");
}

TEST_F(RepoTest, createPrefersRepoLocalConfigOverFallbackConfig)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    auto repoRoot = tempDir.path() / "repo-root";
    ASSERT_TRUE(fs::create_directories(repoRoot));

    auto repoLocalConfig =
      createRepoConfig("repo-local", "repo-local", "https://example.com/repo-local");
    auto fallbackConfig = createRepoConfig("fallback", "fallback", "https://example.com/fallback");
    auto saved = saveConfig(repoLocalConfig, repoRoot / "config.yaml");
    ASSERT_TRUE(saved.has_value()) << saved.error().message();

    auto repo = OSTreeRepo::create(repoRoot, fallbackConfig);
    ASSERT_TRUE(repo.has_value()) << repo.error().message();

    EXPECT_EQ(repo->get()->getConfig().defaultRepo, "repo-local");
    ASSERT_EQ(repo->get()->getConfig().repos.size(), 1);
    EXPECT_EQ(repo->get()->getConfig().repos.front().name, "repo-local");
    EXPECT_EQ(repo->get()->getConfig().repos.front().url, "https://example.com/repo-local");
}

TEST_F(RepoTest, createFailsWhenRepoLocalConfigIsInvalid)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    auto repoRoot = tempDir.path() / "repo-root";
    ASSERT_TRUE(fs::create_directories(repoRoot));

    std::ofstream(repoRoot / "config.yaml") << "invalid: [yaml";

    auto fallbackConfig = createRepoConfig("fallback", "fallback", "https://example.com/fallback");
    auto repo = OSTreeRepo::create(repoRoot, fallbackConfig);

    EXPECT_FALSE(repo.has_value());
}

TEST_F(RepoTest, loadFromPathFailsWhenCacheIsMissingButCreateCanRepairIt)
{
    TempDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    auto repoRoot = tempDir.path() / "repo-root";
    ASSERT_TRUE(fs::create_directories(repoRoot));

    auto config = createRepoConfig();
    auto created = OSTreeRepo::create(repoRoot, config);
    ASSERT_TRUE(created.has_value()) << created.error().message();

    std::error_code ec;
    fs::remove(repoRoot / "states.json", ec);
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_FALSE(fs::exists(repoRoot / "states.json"));

    auto loaded = OSTreeRepo::loadFromPath(repoRoot);
    EXPECT_FALSE(loaded.has_value());

    auto repaired = OSTreeRepo::create(repoRoot, config);
    ASSERT_TRUE(repaired.has_value()) << repaired.error().message();
    EXPECT_TRUE(fs::exists(repoRoot / "states.json"));
}

TEST_F(RepoTest, exportDir)
{
    // 准备测试环境
    TempDir tempDir("repo_test_");
    ASSERT_TRUE(tempDir.isValid()) << "Failed to create temporary directory";
    std::error_code ec;
    bool created = fs::create_directories(tempDir.path(), ec);
    EXPECT_FALSE(ec) << "Error creating directory: " << ec.message();
    EXPECT_TRUE(fs::exists(tempDir.path())) << "Directory not created";

    std::string repoPath = tempDir.path().string();
    std::string ostreeRepoPath = repoPath + "/repo";
    std::string remoteEndpoint = "https://store-llrepo.deepin.com/repos/";
    std::string remoteRepoName = "repo";

    // 初始化配置和repo对象
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(repoPath, config);

    // 创建测试文件和目录结构，包括XDG标准文件
    fs::path srcDirPath = tempDir.path() / "src";
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
    fs::path destDirPath = tempDir.path() / "entries";
    ostreeRepo->wrapGetOverlayShareDirFunc = [destDirPath]() {
        return destDirPath / "share";
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
        return destDirPath / "apps/share";
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
    fs::path emptyDirPath = tempDir.path() / "empty";
    created = fs::create_directories(emptyDirPath, ec);
    EXPECT_TRUE(created) << "Failed to create empty directory";
    EXPECT_FALSE(ec) << "Error creating empty directory: " << ec.message();
    EXPECT_TRUE(fs::exists(emptyDirPath)) << "Empty directory not created";
    fs::path emptyDestPath = tempDir.path() / "empty_dest";
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
          path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(std::unique_ptr<ClientAPIWrapper>,
                createClientV2,
                (const std::string &url),
                (override, const));
};

class OSTreeRepoAccessor : public repo::OSTreeRepo
{
public:
    using repo::OSTreeRepo::buildPullRefCandidates;

    OSTreeRepoAccessor()
        : repo::OSTreeRepo(
          "", api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }
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

TEST(OSTreeRepoTest, BuildPullRefCandidatesFallbackToRuntimeForBinary)
{
    auto ref = package::Reference::parse("stable:org.deepin.demo/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    auto candidates = OSTreeRepoAccessor::buildPullRefCandidates(*ref, "binary");

    ASSERT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates[0], "stable/org.deepin.demo/1.0.0/x86_64/binary");
    EXPECT_EQ(candidates[1], "stable/org.deepin.demo/1.0.0/x86_64/runtime");
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
          path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
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

namespace {

TEST_F(RepoTest, exportAppBinariesCreatesDefaultAndExportedScripts)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = "bin-commit",
        .info =
          api::types::v1::PackageInfoV2{
            .command = std::vector<std::string>{ "myapp" },
            .exportedBinaries = std::vector<std::string>{ "mytool", "myutil" },
            .id = "com.example.app",
            .kind = "app",
            .packageInfoV2Module = "binary",
          },
    };

    auto entriesDir = tempDir.path() / "entries";
    auto result = ostreeRepo->exportAppBinaries(entriesDir, item);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto binDir = entriesDir / "bin";
    // Default script named after appid
    EXPECT_TRUE(fs::exists(binDir / "com.example.app"));
    // Exported binaries scripts
    EXPECT_TRUE(fs::exists(binDir / "mytool"));
    EXPECT_TRUE(fs::exists(binDir / "myutil"));

    // Verify script content
    std::ifstream defaultScript(binDir / "com.example.app");
    std::string content((std::istreambuf_iterator<char>(defaultScript)),
                        std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("exec ll-cli run com.example.app - myapp \"$@\""), std::string::npos);

    // Verify executable permissions (0755)
    std::error_code ec;
    auto perms = fs::status(binDir / "com.example.app", ec).permissions();
    EXPECT_TRUE((perms & fs::perms::owner_exec) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::group_exec) != fs::perms::none);
    EXPECT_TRUE((perms & fs::perms::others_exec) != fs::perms::none);
}

TEST_F(RepoTest, exportAppBinariesSkipsWhenCommandEmpty)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = "no-cmd-commit",
        .info =
          api::types::v1::PackageInfoV2{
            .exportedBinaries = std::vector<std::string>{ "tool" },
            .id = "com.example.nocmd",
            .kind = "app",
            .packageInfoV2Module = "binary",
          },
    };

    auto entriesDir = tempDir.path() / "entries";
    auto result = ostreeRepo->exportAppBinaries(entriesDir, item);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // No scripts should be created when command is empty
    EXPECT_FALSE(fs::exists(entriesDir / "bin" / "com.example.nocmd"));
    EXPECT_FALSE(fs::exists(entriesDir / "bin" / "tool"));
}

TEST_F(RepoTest, exportAppBinariesOExclPreventsOverwrite)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    api::types::v1::RepositoryCacheLayersItem item{
        .commit = "excl-commit",
        .info =
          api::types::v1::PackageInfoV2{
            .command = std::vector<std::string>{ "myapp" },
            .id = "com.example.excl",
            .kind = "app",
            .packageInfoV2Module = "binary",
          },
    };

    auto entriesDir = tempDir.path() / "entries";
    auto result = ostreeRepo->exportAppBinaries(entriesDir, item);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    auto scriptPath = entriesDir / "bin" / "com.example.excl";
    ASSERT_TRUE(fs::exists(scriptPath));

    // Pre-create a file at the same path to verify O_EXCL is used
    // (exportAppBinaries should log warning and continue, not crash)
    auto result2 = ostreeRepo->exportAppBinaries(entriesDir, item);
    ASSERT_TRUE(result2.has_value()) << result2.error().message();

    // Original file should still exist
    EXPECT_TRUE(fs::exists(scriptPath));
}

TEST_F(RepoTest, unexportAppEntriesRemovesBinaryScripts)
{
    TempDir tempDir;
    auto config = api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 };
    auto ostreeRepo = std::make_unique<MockOstreeRepo>(tempDir.path(), config);

    const std::string commit = "unexport-bin-commit";
    auto layerDir = tempDir.path() / "layers" / commit;
    auto source = layerDir / "entries";
    fs::create_directories(source / "share/applications");
    fs::create_directories(layerDir);

    // Write info.json with exportedBinaries (all required fields for PackageInfoV2)
    nlohmann::json infoJson = {
        { "arch", nlohmann::json::array({ "x86_64" }) },
        { "base", "org.deepin.base/23.0.0" },
        { "channel", "main" },
        { "command", nlohmann::json::array({ "myapp" }) },
        { "exportedBinaries", nlohmann::json::array({ "mytool" }) },
        { "id", "com.example.unexport" },
        { "kind", "app" },
        { "module", "binary" },
        { "name", "Test Unexport" },
        { "schema_version", "2" },
        { "size", 0 },
        { "version", "1.0.0" },
    };
    std::ofstream(layerDir / "info.json") << infoJson.dump();

    auto entriesDir = tempDir.path() / "entries";
    auto binDir = entriesDir / "bin";
    fs::create_directories(binDir);

    // Create binary scripts as if they were exported
    std::ofstream(binDir / "com.example.unexport")
      << "#!/bin/sh\nexec ll-cli run com.example.unexport - myapp \"$@\"\n";
    std::ofstream(binDir / "mytool")
      << "#!/bin/sh\nexec ll-cli run com.example.unexport - myapp \"$@\"\n";

    EXPECT_TRUE(fs::exists(binDir / "com.example.unexport"));
    EXPECT_TRUE(fs::exists(binDir / "mytool"));

    auto result = ostreeRepo->unexportAppEntries(entriesDir, { layerDir });
    ASSERT_TRUE(result.has_value()) << result.error().message();

    // Scripts should be removed
    EXPECT_FALSE(fs::exists(binDir / "com.example.unexport"));
    EXPECT_FALSE(fs::exists(binDir / "mytool"));
}

} // namespace
} // namespace

} // namespace linglong::repo::test
