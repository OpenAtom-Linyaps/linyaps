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
using ::testing::InSequence;
using ::testing::Invoke;
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
                clearReferenceLocal,
                (const package::FuzzyReference &fuzzyRef, bool semanticMatching),
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

class MockCli : public cli::Cli
{
public:
    using cli::Cli::Cli;

    MOCK_METHOD(utils::error::Result<repo::OSTreeRepo *>, getRepo, (), (override, noexcept));
};

class RepoAndPackageManagerCli : public cli::Cli
{
public:
    using cli::Cli::Cli;

    utils::error::Result<repo::OSTreeRepo *> callGetRepo() noexcept { return cli::Cli::getRepo(); }

    utils::error::Result<void> callInitializeRepo() noexcept { return cli::Cli::initializeRepo(); }

    utils::error::Result<api::dbus::v1::PackageManager *> callGetPkgMan()
    {
        return cli::Cli::getPkgMan();
    }

    MOCK_METHOD(utils::error::Result<std::unique_ptr<repo::OSTreeRepo>>,
                loadRepoFromPath,
                (const std::filesystem::path &repoRoot),
                (override, noexcept));
    MOCK_METHOD(utils::error::Result<void>, initializeRepo, (), (override, noexcept));
    MOCK_METHOD(utils::error::Result<api::dbus::v1::PackageManager *>, getPkgMan, (), (override));
    MOCK_METHOD(utils::error::Result<std::unique_ptr<api::dbus::v1::PackageManager>>,
                initializePeerModePackageManager,
                (),
                (override));
    MOCK_METHOD(utils::error::Result<std::unique_ptr<api::dbus::v1::PackageManager>>,
                initializeDBusPackageManager,
                (),
                (override));
};

utils::error::Result<std::unique_ptr<repo::OSTreeRepo>>
makeLoadRepoError(const std::string &message)
{
    LINGLONG_TRACE("make load repo error");

    return LINGLONG_ERR(message);
}

utils::error::Result<void> makeVoidError(const std::string &message)
{
    LINGLONG_TRACE("make void error");

    return LINGLONG_ERR(message);
}

utils::error::Result<api::dbus::v1::PackageManager *>
makePackageManagerError(const std::string &message)
{
    LINGLONG_TRACE("make package manager error");

    return LINGLONG_ERR(message);
}

utils::error::Result<std::unique_ptr<api::dbus::v1::PackageManager>>
makePackageManagerInitializationError(const std::string &message)
{
    LINGLONG_TRACE("make package manager initialization error");

    return LINGLONG_ERR(message);
}

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
        cli = std::make_unique<::testing::NiceMock<MockCli>>(*printer,
                                                             *ociCLI,
                                                             *containerBuilder,
                                                             false,
                                                             std::move(notifier),
                                                             nullptr);
        ON_CALL(*cli, getRepo())
          .WillByDefault(Invoke([this]() -> utils::error::Result<repo::OSTreeRepo *> {
              return repo.get();
          }));
    }

    void TearDown() override
    {
        cli.reset();
        printer.reset();
        ociCLI.reset();
        containerBuilder.reset();
        repo.reset();
        tempDir.reset();
    }

    std::unique_ptr<MockPrinter> printer;
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<MockRepo> repo;
    std::unique_ptr<::testing::NiceMock<MockCli>> cli;
};

class CliRepoAndPackageManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        printer = std::make_unique<MockPrinter>();
        tempDir = std::make_unique<TempDir>();
        ociCLI = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        cli = makeCli(false);
    }

    std::unique_ptr<repo::OSTreeRepo> makeRepo(const std::filesystem::path &path)
    {
        return std::make_unique<MockRepo>(path);
    }

    std::unique_ptr<RepoAndPackageManagerCli> makeCli(bool peerMode)
    {
        auto notifier = std::make_unique<cli::DummyNotifier>();
        return std::make_unique<RepoAndPackageManagerCli>(*printer,
                                                          *ociCLI,
                                                          *containerBuilder,
                                                          peerMode,
                                                          std::move(notifier),
                                                          nullptr);
    }

    std::unique_ptr<MockPrinter> printer;
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<RepoAndPackageManagerCli> cli;
};

TEST_F(CliRepoAndPackageManagerTest, getRepoCachesLoadedRepository)
{
    repo::OSTreeRepo *loadedRepo = nullptr;

    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([&](const std::filesystem::path &path)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          auto repo = makeRepo(path);
          loadedRepo = repo.get();
          return repo;
      }));
    EXPECT_CALL(*cli, initializeRepo()).Times(0);

    auto first = cli->callGetRepo();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, loadedRepo);

    auto second = cli->callGetRepo();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, loadedRepo);
}

TEST_F(CliRepoAndPackageManagerTest, getRepoInitializesAndReloadsWhenInitialLoadFails)
{
    repo::OSTreeRepo *loadedRepo = nullptr;
    InSequence seq;

    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([](const std::filesystem::path &)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          return makeLoadRepoError("load failed");
      }));
    EXPECT_CALL(*cli, initializeRepo()).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([&](const std::filesystem::path &path)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          auto repo = makeRepo(path);
          loadedRepo = repo.get();
          return repo;
      }));

    auto result = cli->callGetRepo();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, loadedRepo);
}

TEST_F(CliRepoAndPackageManagerTest, getRepoReturnsInitializationErrorWhenInitializationFails)
{
    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([](const std::filesystem::path &)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          return makeLoadRepoError("load failed");
      }));
    EXPECT_CALL(*cli, initializeRepo()).WillOnce(Invoke([]() -> utils::error::Result<void> {
        return makeVoidError("init failed");
    }));

    auto result = cli->callGetRepo();

    EXPECT_FALSE(result.has_value());
}

TEST_F(CliRepoAndPackageManagerTest, getRepoReturnsReloadErrorAfterInitialization)
{
    InSequence seq;

    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([](const std::filesystem::path &)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          return makeLoadRepoError("load failed");
      }));
    EXPECT_CALL(*cli, initializeRepo()).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([](const std::filesystem::path &)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          return makeLoadRepoError("reload failed");
      }));

    auto result = cli->callGetRepo();

    EXPECT_FALSE(result.has_value());
}

TEST_F(CliRepoAndPackageManagerTest, initializeRepoSucceedsWhenPackageManagerIsAvailable)
{
    EXPECT_CALL(*cli, getPkgMan())
      .WillOnce(Return(utils::error::Result<api::dbus::v1::PackageManager *>{ nullptr }));

    auto result = cli->callInitializeRepo();

    EXPECT_TRUE(result.has_value());
}

TEST_F(CliRepoAndPackageManagerTest, initializeRepoReturnsErrorWhenPackageManagerCannotStart)
{
    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Invoke([]() {
        return makePackageManagerError("start package manager failed");
    }));

    auto result = cli->callInitializeRepo();

    EXPECT_FALSE(result.has_value());
}

TEST_F(CliRepoAndPackageManagerTest, getPkgManUsesDBusPackageManagerWhenPeerModeIsDisabled)
{
    auto cli = makeCli(false);

    EXPECT_CALL(*cli, initializePeerModePackageManager()).Times(0);
    EXPECT_CALL(*cli, initializeDBusPackageManager()).WillOnce(Invoke([]() {
        return makePackageManagerInitializationError("dbus package manager failed");
    }));

    auto result = cli->callGetPkgMan();

    EXPECT_FALSE(result.has_value());
}

TEST_F(CliRepoAndPackageManagerTest, getPkgManUsesPeerModePackageManagerWhenPeerModeIsEnabled)
{
    auto cli = makeCli(true);

    EXPECT_CALL(*cli, initializeDBusPackageManager()).Times(0);
    EXPECT_CALL(*cli, initializePeerModePackageManager()).WillOnce(Invoke([]() {
        return makePackageManagerInitializationError("peer package manager failed");
    }));

    auto result = cli->callGetPkgMan();

    EXPECT_FALSE(result.has_value());
}

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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
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
