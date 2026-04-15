/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/cli_printer.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/printer.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <filesystem>
#include <fstream>

using namespace linglong;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

namespace {

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    std::filesystem::path defaultSharedDir() const noexcept { return this->getDefaultSharedDir(); }

    std::filesystem::path overlaySharedDir() const noexcept { return this->getOverlayShareDir(); }

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>,
                listLocal,
                (),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReference,
                (const package::FuzzyReference &fuzzyRef,
                 const repo::clearReferenceOption &opts,
                 const std::string &module,
                 const std::optional<std::string> &repo),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref,
                 std::string module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));
};

class MockPrinter : public cli::CLIPrinter
{
public:
    MOCK_METHOD(void,
                printUpgradeList,
                (std::vector<api::types::v1::UpgradeListResult> &),
                (override));
    MOCK_METHOD(void, printContent, (const QStringList &filePaths), (override));
};

class CliTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        printer = std::make_unique<MockPrinter>();
        tempDir = std::make_unique<TempDir>();
        ociCLI = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        repo = std::make_unique<MockRepo>(tempDir->path());
        auto notifier = std::make_unique<cli::DummyNotifier>();
        cli = std::make_unique<cli::Cli>(*printer,
                                         *ociCLI,
                                         *containerBuilder,
                                         false,
                                         *repo,
                                         std::move(notifier),
                                         nullptr);
    }

    void TearDown() override
    {
        printer.reset();
        tempDir.reset();
        ociCLI.reset();
        containerBuilder.reset();
        repo.reset();
        cli.reset();
    }

    std::unique_ptr<MockPrinter> printer;
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<MockRepo> repo;
    std::unique_ptr<cli::Cli> cli;
};

TEST_F(CliTest, listUpgradableOnlyApp)
{
    EXPECT_CALL(*repo, listLocal())
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
      } }));

    EXPECT_CALL(*repo, latestRemoteReference(_))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo1" },
        .reference = package::Reference::parse("main:id1/2.0.0/x86_64").value() }));

    EXPECT_CALL(*printer,
                printUpgradeList(ElementsAre(api::types::v1::UpgradeListResult{
                  .id = "id1",
                  .newVersion = "2.0.0",
                  .oldVersion = "1.0.0",
                })))
      .WillOnce(Return());
    cli->list(cli::ListOptions{ .showUpgradeList = true });
}

TEST_F(CliTest, listUpgradableMultiKind)
{
    EXPECT_CALL(*repo, listLocal())
      .WillOnce(Return(
        std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                                                      .arch = std::vector<std::string>{ "x86_64" },
                                                      .channel = "main",
                                                      .id = "id1",
                                                      .kind = "app",
                                                      .packageInfoV2Module = "binary",
                                                      .version = "1.0.0",
                                                    },
                                                    api::types::v1::PackageInfoV2{
                                                      .arch = std::vector<std::string>{ "x86_64" },
                                                      .channel = "main",
                                                      .id = "id2",
                                                      .kind = "runtime",
                                                      .packageInfoV2Module = "binary",
                                                      .version = "1.0.0",
                                                    } }));

    EXPECT_CALL(*repo, latestRemoteReference(_))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo1" },
        .reference = package::Reference::parse("main:id1/2.0.0/x86_64").value() }));

    EXPECT_CALL(*printer,
                printUpgradeList(ElementsAre(api::types::v1::UpgradeListResult{
                  .id = "id1",
                  .newVersion = "2.0.0",
                  .oldVersion = "1.0.0",
                })))
      .WillOnce(Return());
    cli->list(cli::ListOptions{ .showUpgradeList = true });
}

TEST_F(CliTest, listUpgradableMultiApp)
{
    LINGLONG_TRACE("listUpgradableMultiApp");

    EXPECT_CALL(*repo, listLocal())
      .WillOnce(Return(
        std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
                                                      .arch = std::vector<std::string>{ "x86_64" },
                                                      .channel = "main",
                                                      .id = "id1",
                                                      .kind = "app",
                                                      .packageInfoV2Module = "binary",
                                                      .version = "1.0.0",
                                                    },
                                                    api::types::v1::PackageInfoV2{
                                                      .arch = std::vector<std::string>{ "x86_64" },
                                                      .channel = "main",
                                                      .id = "id2",
                                                      .kind = "app",
                                                      .packageInfoV2Module = "binary",
                                                      .version = "1.0.0",
                                                    },
                                                    api::types::v1::PackageInfoV2{
                                                      .arch = std::vector<std::string>{ "x86_64" },
                                                      .channel = "private",
                                                      .id = "id2",
                                                      .kind = "app",
                                                      .packageInfoV2Module = "binary",
                                                      .version = "1.0.0",
                                                    } }));

    EXPECT_CALL(*repo, latestRemoteReference(_))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo1" },
        .reference = package::Reference::parse("main:id1/1.0.0/x86_64").value() }))
      .WillOnce(Return(LINGLONG_ERR("")))
      .WillOnce(Return(package::ReferenceWithRepo{
        .repo = api::types::v1::Repo{ .name = "repo1" },
        .reference = package::Reference::parse("private:id2/2.0.0/x86_64").value() }));

    EXPECT_CALL(*printer,
                printUpgradeList(ElementsAre(api::types::v1::UpgradeListResult{
                  .id = "id2",
                  .newVersion = "2.0.0",
                  .oldVersion = "1.0.0",
                })))
      .WillOnce(Return());
    cli->list(cli::ListOptions{ .showUpgradeList = true });
}

TEST_F(CliTest, listUpgradableEmpty)
{
    EXPECT_CALL(*repo, listLocal()).WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{}));

    EXPECT_CALL(*printer, printUpgradeList(IsEmpty())).WillOnce(Return());
    cli->list(cli::ListOptions{ .showUpgradeList = true });
}

TEST_F(CliTest, listUpgradableNoUpgrade)
{
    LINGLONG_TRACE("listUpgradableMultiApp");

    EXPECT_CALL(*repo, listLocal())
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{ api::types::v1::PackageInfoV2{
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
      } }));

    EXPECT_CALL(*repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("")));

    EXPECT_CALL(*printer, printUpgradeList(IsEmpty())).WillOnce(Return());
    cli->list(cli::ListOptions{ .showUpgradeList = true });
}

TEST_F(CliTest, contentPreferDesktopFromDefaultSharedDir)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-default";
    const auto layerEntriesDir =
      tempDir->path() / "layers" / commit / "entries" / "share" / "applications";
    const auto defaultDesktopPath =
      repo->defaultSharedDir() / "applications" / "org.example.app.desktop";

    std::filesystem::create_directories(layerEntriesDir);
    std::filesystem::create_directories(defaultDesktopPath.parent_path());
    std::ofstream(layerEntriesDir / "org.example.app.desktop") << "Test desktop content";
    std::ofstream(defaultDesktopPath) << "Test desktop content in default";

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer,
                printContent(ElementsAre(QString::fromStdString(defaultDesktopPath.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST_F(CliTest, contentResolvesDesktopFromSymlinkedEntriesShareDir)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-symlinked-share";
    const auto layerFilesDir =
      tempDir->path() / "layers" / commit / "files" / "share" / "applications";
    const auto layerEntriesShareLink = tempDir->path() / "layers" / commit / "entries" / "share";
    const auto defaultDesktopPath =
      repo->defaultSharedDir() / "applications" / "org.example.app.desktop";

    std::filesystem::create_directories(layerFilesDir);
    std::filesystem::create_directories(layerEntriesShareLink.parent_path());
    std::filesystem::create_directories(defaultDesktopPath.parent_path());
    std::ofstream(layerFilesDir / "org.example.app.desktop") << "Test desktop content";
    std::ofstream(defaultDesktopPath) << "Test desktop content in default";

    std::error_code ec;
    std::filesystem::create_directory_symlink("../files/share", layerEntriesShareLink, ec);
    if (ec) {
        GTEST_SKIP() << "directory symlink support is required: " << ec.message();
    }

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer,
                printContent(ElementsAre(QString::fromStdString(defaultDesktopPath.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST_F(CliTest, contentResolvesOverlayDesktopFromSymlinkedEntriesShareDir)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-symlinked-share-overlay";
    const auto layerFilesDir =
      tempDir->path() / "layers" / commit / "files" / "share" / "applications";
    const auto layerEntriesShareLink = tempDir->path() / "layers" / commit / "entries" / "share";
    const auto overlayDesktopPath =
      repo->overlaySharedDir() / "applications" / "org.example.app.desktop";

    std::filesystem::create_directories(layerFilesDir);
    std::filesystem::create_directories(layerEntriesShareLink.parent_path());
    std::filesystem::create_directories(overlayDesktopPath.parent_path());
    std::ofstream(layerFilesDir / "org.example.app.desktop") << "Test desktop content";
    std::ofstream(overlayDesktopPath) << "Test desktop content in overlay";

    std::error_code ec;
    std::filesystem::create_directory_symlink("../files/share", layerEntriesShareLink, ec);
    if (ec) {
        GTEST_SKIP() << "directory symlink support is required: " << ec.message();
    }

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer,
                printContent(ElementsAre(QString::fromStdString(overlayDesktopPath.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST_F(CliTest, contentFallbackDesktopToOverlaySharedDir)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-overlay";
    const auto layerEntriesDir =
      tempDir->path() / "layers" / commit / "entries" / "share" / "applications";
    const auto overlayDesktopPath =
      repo->overlaySharedDir() / "applications" / "org.example.app.desktop";

    std::filesystem::create_directories(layerEntriesDir);
    std::filesystem::create_directories(overlayDesktopPath.parent_path());
    std::ofstream(layerEntriesDir / "org.example.app.desktop") << "Test desktop content";
    std::ofstream(overlayDesktopPath) << "Test desktop content in overlay";

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer,
                printContent(ElementsAre(QString::fromStdString(overlayDesktopPath.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST_F(CliTest, contentMapsLegacySystemdUserPathToExportedLibPath)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-systemd-legacy";
    const auto layerEntriesDir =
      tempDir->path() / "layers" / commit / "entries" / "share" / "systemd" / "user";
    const auto exportedDir = tempDir->path() / "entries" / "lib" / "systemd" / "user";
    const auto exportedFile = exportedDir / "org.example.app.service";

    std::filesystem::create_directories(layerEntriesDir);
    std::filesystem::create_directories(exportedDir);
    std::ofstream(layerEntriesDir / "org.example.app.service")
      << "[Service]\nExecStart=/bin/true\n";
    std::ofstream(exportedFile) << "[Service]\nExecStart=/bin/true\n";

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer, printContent(ElementsAre(QString::fromStdString(exportedFile.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST_F(CliTest, contentPrefersLibSystemdUserOverLegacySharePath)
{
    auto ref = package::Reference::parse("main:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(ref.has_value());

    const std::string commit = "test-commit-systemd-lib";
    const auto layerLibDir =
      tempDir->path() / "layers" / commit / "entries" / "lib" / "systemd" / "user";
    const auto layerLegacyDir =
      tempDir->path() / "layers" / commit / "entries" / "share" / "systemd" / "user";
    const auto exportedDir = tempDir->path() / "entries" / "lib" / "systemd" / "user";
    const auto exportedFile = exportedDir / "org.example.app.service";

    std::filesystem::create_directories(layerLibDir);
    std::filesystem::create_directories(layerLegacyDir);
    std::filesystem::create_directories(exportedDir);
    std::ofstream(layerLibDir / "org.example.app.service") << "[Service]\nExecStart=/bin/true\n";
    std::ofstream(layerLegacyDir / "org.example.app.service") << "[Service]\nExecStart=/bin/true\n";
    std::ofstream(exportedFile) << "[Service]\nExecStart=/bin/true\n";

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.commit = commit;
    layerItem.info.kind = "app";

    EXPECT_CALL(*repo, clearReference(_, _, _, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _, _))
      .WillRepeatedly(
        [layerItem](const package::Reference &, std::string, const std::optional<std::string> &)
          -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
            return layerItem;
        });
    EXPECT_CALL(*printer, printContent(ElementsAre(QString::fromStdString(exportedFile.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

} // namespace
