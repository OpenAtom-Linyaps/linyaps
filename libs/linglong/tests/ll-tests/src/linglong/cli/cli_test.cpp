/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/cli.h"
#include "linglong/cli/cli_printer.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/printer.h"
#include "linglong/package/layer_dir.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <vector>

#include <unistd.h>

using namespace linglong;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;

namespace {

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path,
             api::types::v1::RepoConfigV2 config =
               api::types::v1::RepoConfigV2{
                 .defaultRepo = "",
                 .repos = {},
                 .version = 2,
               })
        : repo::OSTreeRepo(path, std::move(config))
    {
    }

    bool initialize() { return this->init(true).has_value(); }

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
                (const package::Reference &ref, std::string module),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<repo::RemotePackages>,
                matchRemoteByPriority,
                (const package::FuzzyReference &fuzzyRef,
                 const std::optional<api::types::v1::Repo> &repo),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<api::types::v1::PackageInfoV2>,
                fetchRemotePackageInfo,
                (const package::ReferenceWithRepo &refRepo, const std::string &module),
                (override, noexcept));
};

class MockPrinter : public cli::CLIPrinter
{
public:
    MOCK_METHOD(void, printErr, (const utils::error::Error &error), (override));
    MOCK_METHOD(void,
                printUpgradeList,
                (std::vector<api::types::v1::UpgradeListResult> &),
                (override));
    MOCK_METHOD(void, printContent, (const QStringList &filePaths), (override));
    MOCK_METHOD(void, printProgress, (double percentage, const std::string &message), (override));
    MOCK_METHOD(void, printMessage, (const std::string &message), (override));
    MOCK_METHOD(void, finishProgress, (), (override));
    MOCK_METHOD(void, printPackage, (const api::types::v1::PackageInfoV2 &info), (override));
    MOCK_METHOD(void,
                printPackages,
                (const std::vector<api::types::v1::PackageInfoDisplay> &packages),
                (override));
    MOCK_METHOD(void,
                printModuleSizes,
                (const std::vector<ModuleSizeInfo> &sizes,
                 std::uint64_t actualTotalSize,
                 std::uint64_t repoSize),
                (override));
    MOCK_METHOD(void, printDepends, (const std::vector<DependsNode> &depends), (override));
    MOCK_METHOD(void,
                printContainers,
                (const std::vector<api::types::v1::CliContainer> &containers),
                (override));
};

class MockCli : public cli::Cli
{
public:
    using cli::Cli::Cli;

    MOCK_METHOD(utils::error::Result<repo::OSTreeRepo *>, getRepo, (bool), (override, noexcept));
    MOCK_METHOD(utils::error::Result<api::dbus::v1::PackageManager *>, getPkgMan, (), (override));
};

class RepoAndPackageManagerCli : public cli::Cli
{
public:
    using cli::Cli::Cli;

    utils::error::Result<repo::OSTreeRepo *> callGetRepo(bool forceReload = false) noexcept
    {
        return cli::Cli::getRepo(forceReload);
    }

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

api::types::v1::PackageInfoV2 makeRemotePackageInfo()
{
    return api::types::v1::PackageInfoV2{
        .arch = { "x86_64" },
        .channel = "main",
        .id = "org.example.App",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .name = "Example App",
        .version = "1.0.0",
    };
}

repo::RemotePackages makeRemotePackages(api::types::v1::PackageInfoV2 info)
{
    repo::RemotePackages packages;
    packages.addPackages(
      api::types::v1::Repo{ .name = "stable", .priority = 0, .url = "https://example.com/repo" },
      { std::move(info) });
    return packages;
}

utils::error::Result<repo::RemotePackages> makeRemotePackagesError(const std::string &message)
{
    LINGLONG_TRACE("make remote packages error");

    return LINGLONG_ERR(message);
}

utils::error::Result<api::types::v1::PackageInfoV2>
makeRemotePackageInfoError(const std::string &message)
{
    LINGLONG_TRACE("make remote package info error");

    return LINGLONG_ERR(message);
}

utils::error::Result<package::Reference> makeRepoReferenceError(const std::string &message)
{
    LINGLONG_TRACE("make repo reference error");

    return LINGLONG_ERR(message);
}

utils::error::Result<api::types::v1::RepositoryCacheLayersItem>
makeLayerItemError(const std::string &message)
{
    LINGLONG_TRACE("make layer item error");

    return LINGLONG_ERR(message);
}

utils::error::Result<repo::OSTreeRepo *> makeGetRepoError(const std::string &message)
{
    LINGLONG_TRACE("make get repo error");

    return LINGLONG_ERR(message);
}

struct ImportedLayer
{
    package::Reference reference;
    api::types::v1::RepositoryCacheLayersItem item;
};

std::optional<ImportedLayer> importLayer(repo::OSTreeRepo &targetRepo,
                                         const std::filesystem::path &workspace,
                                         const std::string &id,
                                         const std::string &version,
                                         const std::string &kind,
                                         const std::string &module,
                                         const std::optional<std::string> &base = std::nullopt,
                                         const std::optional<std::string> &runtime = std::nullopt)
{
    const auto layerDir = workspace / (id + "-" + module);
    std::error_code ec;
    std::filesystem::remove_all(layerDir, ec);
    ec.clear();
    std::filesystem::create_directories(layerDir, ec);
    if (ec) {
        ADD_FAILURE() << "failed to create " << layerDir << ": " << ec.message();
        return std::nullopt;
    }

    api::types::v1::PackageInfoV2 info;
    info.arch = std::vector<std::string>{ "x86_64" };
    info.base = base.value_or(std::string{});
    info.channel = "stable";
    info.command = std::vector<std::string>{ "echo", id };
    info.id = id;
    info.kind = kind;
    info.packageInfoV2Module = module;
    info.runtime = runtime;
    info.version = version;
    std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();

    auto imported = targetRepo.importLayerDir(package::LayerDir{ layerDir });
    if (!imported) {
        ADD_FAILURE() << "failed to import " << id << "/" << module << ": "
                      << imported.error().message();
        return std::nullopt;
    }

    auto ref = package::Reference::parse("stable:" + id + "/" + version + "/x86_64");
    if (!ref) {
        ADD_FAILURE() << ref.error().message();
        return std::nullopt;
    }

    return ImportedLayer{ .reference = *ref, .item = std::move(*imported) };
}

class CliTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        printer = std::make_unique<MockPrinter>();
        tempDir = std::make_unique<TempDir>();
        ociCLI = ocppi::cli::crun::Crun::New("/bin/false").value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        repo = std::make_unique<MockRepo>(tempDir->path());
        auto notifier = std::make_unique<cli::DummyNotifier>();
        cli = std::make_unique<::testing::NiceMock<MockCli>>(*printer,
                                                             *ociCLI,
                                                             *containerBuilder,
                                                             false,
                                                             std::move(notifier),
                                                             nullptr);
        ON_CALL(*cli, getRepo(testing::_))
          .WillByDefault(Invoke([this](bool) -> utils::error::Result<repo::OSTreeRepo *> {
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
        ociCLI = ocppi::cli::crun::Crun::New("/bin/false").value();
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

TEST_F(CliTest, installRejectsMissingExplicitLocalPath)
{
    const auto packagePath = tempDir->path() / "net.example_1.0_x86_64_binary";

    EXPECT_CALL(*cli, getPkgMan()).Times(0);
    EXPECT_CALL(*printer, printErr(_))
      .WillOnce(Invoke([&packagePath](const utils::error::Error &error) {
          EXPECT_EQ(error.code(), static_cast<int>(utils::error::ErrorCode::Failed));
          EXPECT_THAT(error.message(), HasSubstr(packagePath.string()));
          EXPECT_THAT(error.message(), HasSubstr("does not exist"));
      }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = packagePath.string() }), -1);
}

TEST_F(CliTest, installRejectsLocalDirectoryBeforeParsingReference)
{
    const auto packagePath = tempDir->path() / "net.example_1.0_x86_64_binary";
    ASSERT_TRUE(std::filesystem::create_directory(packagePath));

    EXPECT_CALL(*cli, getPkgMan()).Times(0);
    EXPECT_CALL(*printer, printErr(_))
      .WillOnce(Invoke([&packagePath](const utils::error::Error &error) {
          EXPECT_EQ(error.code(),
                    static_cast<int>(utils::error::ErrorCode::AppInstallUnsupportedFileFormat));
          EXPECT_THAT(error.message(), HasSubstr(packagePath.string()));
          EXPECT_THAT(error.message(), HasSubstr("not a regular file"));
      }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = packagePath.string() }), -1);
}

TEST_F(CliTest, installRejectsUnsupportedLocalFileBeforeContactingPackageManager)
{
    const auto packagePath = tempDir->path() / "net.example_1.0_x86_64_binary";
    std::ofstream(packagePath) << "not a package";

    EXPECT_CALL(*cli, getPkgMan()).Times(0);
    EXPECT_CALL(*printer, printErr(_))
      .WillOnce(Invoke([&packagePath](const utils::error::Error &error) {
          EXPECT_EQ(error.code(),
                    static_cast<int>(utils::error::ErrorCode::AppInstallUnsupportedFileFormat));
          EXPECT_THAT(error.message(), HasSubstr(packagePath.string()));
          EXPECT_THAT(error.message(), HasSubstr("Unsupported file format"));
      }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = packagePath.string() }), -1);
}

TEST_F(CliTest, installKeepsRemoteVersionReference)
{
    EXPECT_CALL(*cli, getPkgMan())
      .WillOnce(Invoke([]() -> utils::error::Result<api::dbus::v1::PackageManager *> {
          return makePackageManagerError("package manager unavailable");
      }));
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("package manager unavailable"));
    }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.App/1.0.0" }), -1);
}

TEST_F(CliTest, installKeepsRemoteReferencesEndingWithPackageFileSuffix)
{
    EXPECT_CALL(*cli, getPkgMan())
      .Times(2)
      .WillRepeatedly(Invoke([]() -> utils::error::Result<api::dbus::v1::PackageManager *> {
          return makePackageManagerError("package manager unavailable");
      }));
    EXPECT_CALL(*printer, printErr(_)).Times(2);

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.layer" }), -1);
    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = "org.example.uab/1.0.0" }), -1);
}

TEST_F(CliTest, installRecognizesExistingLayerFile)
{
    const auto packagePath = tempDir->path() / "net.example_1.0_x86_64_binary.layer";
    std::ofstream(packagePath) << "layer placeholder";

    EXPECT_CALL(*cli, getPkgMan())
      .WillOnce(Invoke([]() -> utils::error::Result<api::dbus::v1::PackageManager *> {
          return makePackageManagerError("package manager unavailable");
      }));
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("package manager unavailable"));
    }));

    EXPECT_EQ(cli->install(cli::InstallOptions{ .appid = packagePath.string() }), -1);
}

TEST_F(CliTest, taskEventsDriveProgressAndTextOutput)
{
    EXPECT_CALL(*printer, printProgress(testing::DoubleEq(42.0), "fetching"));
    EXPECT_TRUE(
      QMetaObject::invokeMethod(cli.get(),
                                "onTaskEvent",
                                Qt::DirectConnection,
                                Q_ARG(QString, QStringLiteral("state")),
                                Q_ARG(QVariantMap,
                                      common::serialize::toQVariantMap(api::types::v1::TaskState{
                                        .message = "fetching",
                                        .progress = 42.0,
                                        .state = api::types::v1::State::Processing,
                                      }))));

    testing::InSequence sequence;
    EXPECT_CALL(*printer, finishProgress());
    EXPECT_CALL(*printer, printMessage("a standalone message"));
    EXPECT_TRUE(QMetaObject::invokeMethod(
      cli.get(),
      "onTaskEvent",
      Qt::DirectConnection,
      Q_ARG(QString, QStringLiteral("message")),
      Q_ARG(
        QVariantMap,
        QVariantMap({ { QStringLiteral("message"), QStringLiteral("a standalone message") } }))));
}

TEST_F(CliTest, taskFinishedLeavesOutputToCommandFlow)
{
    EXPECT_CALL(*printer, printProgress(_, _)).Times(0);
    EXPECT_TRUE(
      QMetaObject::invokeMethod(cli.get(),
                                "onTaskEvent",
                                Qt::DirectConnection,
                                Q_ARG(QString, QStringLiteral("state")),
                                Q_ARG(QVariantMap,
                                      common::serialize::toQVariantMap(api::types::v1::TaskState{
                                        .message = "",
                                        .progress = 0.0,
                                        .state = api::types::v1::State::Succeed,
                                      }))));

    EXPECT_CALL(*printer, finishProgress()).Times(0);
    EXPECT_CALL(*printer, printMessage(_)).Times(0);
    const auto result = common::serialize::toQVariantMap(api::types::v1::CommonResult{
      .code = 0,
      .message = "installed",
    });
    EXPECT_TRUE(QMetaObject::invokeMethod(cli.get(),
                                          "onTaskFinished",
                                          Qt::DirectConnection,
                                          Q_ARG(QVariantMap, result)));
}

TEST_F(CliTest, failedTaskStateDoesNotPrintErrorAsProgress)
{
    EXPECT_CALL(*printer, printProgress(_, _)).Times(0);
    EXPECT_TRUE(
      QMetaObject::invokeMethod(cli.get(),
                                "onTaskEvent",
                                Qt::DirectConnection,
                                Q_ARG(QString, QStringLiteral("state")),
                                Q_ARG(QVariantMap,
                                      common::serialize::toQVariantMap(api::types::v1::TaskState{
                                        .message = "installation failed",
                                        .progress = 42.0,
                                        .state = api::types::v1::State::Failed,
                                      }))));
}

TEST_F(CliTest, remoteInfoPrintsFetchedPackageFromSpecifiedRepo)
{
    repo = std::make_unique<MockRepo>(
      tempDir->path() / "remote-info-repo",
      api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .alias = "primary",
                                         .name = "stable",
                                         .priority = 0,
                                         .url = "https://example.com/repo" } },
        .version = 2,
      });
    const auto fetchedInfo = makeRemotePackageInfo();

    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce(Invoke([](const package::FuzzyReference &fuzzyRef,
                          const std::optional<api::types::v1::Repo> &specifiedRepo)
                         -> utils::error::Result<repo::RemotePackages> {
          EXPECT_EQ(fuzzyRef.id, "org.example.App");
          EXPECT_TRUE(specifiedRepo.has_value());
          if (specifiedRepo) {
              EXPECT_EQ(specifiedRepo->name, "stable");
          }
          return makeRemotePackages(makeRemotePackageInfo());
      }));
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, "binary"))
      .WillOnce(Invoke(
        [&fetchedInfo](const package::ReferenceWithRepo &refRepo,
                       const std::string &) -> utils::error::Result<api::types::v1::PackageInfoV2> {
            EXPECT_EQ(refRepo.repo.name, "stable");
            EXPECT_EQ(refRepo.reference.toString(), "main:org.example.App/1.0.0/x86_64");
            return fetchedInfo;
        }));
    EXPECT_CALL(*printer, printPackage(_))
      .WillOnce(Invoke([&fetchedInfo](const api::types::v1::PackageInfoV2 &info) {
          EXPECT_EQ(info.id, fetchedInfo.id);
          EXPECT_EQ(info.version, fetchedInfo.version);
      }));
    EXPECT_CALL(*printer, printErr(_)).Times(0);

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App", .repo = "primary" }),
              0);
}

TEST_F(CliTest, remoteInfoRejectsUnknownRepo)
{
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, _)).Times(0);
    EXPECT_CALL(*printer, printPackage(_)).Times(0);
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("repo missing not found"));
    }));

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App", .repo = "missing" }),
              -1);
}

TEST_F(CliTest, remoteInfoReportsRemoteMatchFailure)
{
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce(Invoke([](const package::FuzzyReference &,
                          const std::optional<api::types::v1::Repo> &specifiedRepo)
                         -> utils::error::Result<repo::RemotePackages> {
          EXPECT_FALSE(specifiedRepo.has_value());
          return makeRemotePackagesError("remote search failed");
      }));
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, _)).Times(0);
    EXPECT_CALL(*printer, printPackage(_)).Times(0);
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("remote search failed"));
    }));

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliTest, remoteInfoReportsMissingPackage)
{
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce(Invoke([](const package::FuzzyReference &,
                          const std::optional<api::types::v1::Repo> &specifiedRepo)
                         -> utils::error::Result<repo::RemotePackages> {
          EXPECT_FALSE(specifiedRepo.has_value());
          return repo::RemotePackages{};
      }));
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, _)).Times(0);
    EXPECT_CALL(*printer, printPackage(_)).Times(0);
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("Cannot find such application from remote"));
    }));

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliTest, remoteInfoRejectsInvalidPackageReference)
{
    auto invalidInfo = makeRemotePackageInfo();
    invalidInfo.arch.clear();
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce(Invoke([invalidInfo](const package::FuzzyReference &,
                                     const std::optional<api::types::v1::Repo> &specifiedRepo)
                         -> utils::error::Result<repo::RemotePackages> {
          EXPECT_FALSE(specifiedRepo.has_value());
          return makeRemotePackages(invalidInfo);
      }));
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, _)).Times(0);
    EXPECT_CALL(*printer, printPackage(_)).Times(0);
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("missing architecture"));
    }));

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App" }), -1);
}

TEST_F(CliTest, remoteInfoReportsFetchFailure)
{
    EXPECT_CALL(*repo, matchRemoteByPriority(_, _))
      .WillOnce(Invoke([](const package::FuzzyReference &,
                          const std::optional<api::types::v1::Repo> &specifiedRepo)
                         -> utils::error::Result<repo::RemotePackages> {
          EXPECT_FALSE(specifiedRepo.has_value());
          return makeRemotePackages(makeRemotePackageInfo());
      }));
    EXPECT_CALL(*repo, fetchRemotePackageInfo(_, "binary"))
      .WillOnce(
        Invoke([](const package::ReferenceWithRepo &,
                  const std::string &) -> utils::error::Result<api::types::v1::PackageInfoV2> {
            return makeRemotePackageInfoError("fetch package info failed");
        }));
    EXPECT_CALL(*printer, printPackage(_)).Times(0);
    EXPECT_CALL(*printer, printErr(_)).WillOnce(Invoke([](const utils::error::Error &error) {
        EXPECT_THAT(error.message(), HasSubstr("fetch package info failed"));
    }));

    EXPECT_EQ(cli->remoteInfo(cli::InspectOptions{ .appid = "org.example.App" }), -1);
}

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

TEST_F(CliRepoAndPackageManagerTest, getRepoForceReloadReloadsRepository)
{
    repo::OSTreeRepo *firstRepo = nullptr;
    repo::OSTreeRepo *secondRepo = nullptr;

    EXPECT_CALL(*cli, loadRepoFromPath(_))
      .WillOnce(Invoke([&](const std::filesystem::path &path)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          auto repo = makeRepo(path);
          firstRepo = repo.get();
          return repo;
      }))
      .WillOnce(Invoke([&](const std::filesystem::path &path)
                         -> utils::error::Result<std::unique_ptr<repo::OSTreeRepo>> {
          auto repo = makeRepo(path);
          secondRepo = repo.get();
          return repo;
      }));
    EXPECT_CALL(*cli, initializeRepo()).Times(0);

    auto first = cli->callGetRepo();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, firstRepo);

    auto second = cli->callGetRepo(true);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, secondRepo);
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
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
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly([layerItem](const package::Reference &, std::string)
                        -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
          return layerItem;
      });
    EXPECT_CALL(*printer, printContent(ElementsAre(QString::fromStdString(exportedFile.string()))))
      .WillOnce(Return());

    EXPECT_EQ(cli->content(cli::ContentOptions{ .appid = "org.example.app" }), 0);
}

TEST(Cli, ErrorCodeFromDBusMapsAccessDeniedOnly)
{
    EXPECT_EQ(cli::errorCodeFromDBus(QDBusError::AccessDenied),
              utils::error::ErrorCode::PermissionDenied);
    EXPECT_EQ(cli::errorCodeFromDBus(QDBusError::Failed), utils::error::ErrorCode::Failed);
    EXPECT_EQ(cli::errorCodeFromDBus(QDBusError::NoMemory), utils::error::ErrorCode::Failed);
}

TEST_F(CliTest, searchReportsErrorWhenNoRepoConfigured)
{
    cli::SearchOptions options;
    options.appid = "org.example";
    options.type = "all";

    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->search(options), -1);
}

TEST_F(CliTest, searchReportsErrorWhenSpecifiedRepoDoesNotExist)
{
    repo = std::make_unique<MockRepo>(
      tempDir->path() / "search-repo",
      api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .alias = "primary",
                                         .name = "stable",
                                         .priority = 0,
                                         .url = "https://example.com/repo" } },
        .version = 2,
      });

    cli::SearchOptions options;
    options.appid = "org.example";
    options.repo = "does-not-exist";

    EXPECT_CALL(*printer, printErr(::testing::_))
      .WillOnce(::testing::Invoke([](const utils::error::Error &error) {
          EXPECT_THAT(error.message(), HasSubstr("repo does-not-exist not found"));
      }));
    EXPECT_EQ(cli->search(options), -1);
}

TEST_F(CliTest, upgradeRejectsInvalidAppid)
{
    cli::UpgradeOptions options;
    options.appid = "org.example/1.0.0/x86_64/extra";

    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->upgrade(options), -1);
}

TEST_F(CliTest, upgradeReportsNotInstalledWhenLocalRefCannotBeResolved)
{
    cli::UpgradeOptions options;
    options.appid = "main:org.example/1.0.0/x86_64";

    EXPECT_CALL(*repo, clearReferenceLocal(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(makeRepoReferenceError("not installed")));
    EXPECT_CALL(*printer, printMessage(::testing::HasSubstr("not installed"))).Times(1);
    EXPECT_EQ(cli->upgrade(options), -1);
}

TEST_F(CliTest, upgradeReportsLayerItemError)
{
    cli::UpgradeOptions options;
    options.appid = "main:org.example/1.0.0/x86_64";

    package::Reference ref = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    EXPECT_CALL(*repo, clearReferenceLocal(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(ref));
    EXPECT_CALL(*repo, getLayerItem(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(makeLayerItemError("layer error")));
    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->upgrade(options), -1);
}

TEST_F(CliTest, upgradeRejectsNonAppTarget)
{
    cli::UpgradeOptions options;
    options.appid = "main:org.example/1.0.0/x86_64";

    api::types::v1::RepositoryCacheLayersItem layerItem;
    layerItem.info.kind = "runtime";
    layerItem.commit = "commit-1";

    package::Reference ref = package::Reference::parse("main:org.example/1.0.0/x86_64").value();
    EXPECT_CALL(*repo, clearReferenceLocal(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(ref));
    EXPECT_CALL(*repo, getLayerItem(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(layerItem));
    EXPECT_CALL(*printer, printMessage(::testing::HasSubstr("not an application"))).Times(1);
    EXPECT_EQ(cli->upgrade(options), -1);
}

TEST_F(CliTest, uninstallRejectsInvalidAppid)
{
    cli::UninstallOptions options;
    options.appid = "org.example/1.0.0/x86_64/extra";

    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->uninstall(options), -1);
}

TEST_F(CliTest, listReportsRepoError)
{
    cli::ListOptions options;

    EXPECT_CALL(*cli, getRepo(::testing::_))
      .WillOnce(::testing::Return(makeGetRepoError("no repo")));
    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->list(options), -1);
}

TEST_F(CliTest, sizeReportsRepoError)
{
    cli::SizeOptions options;

    EXPECT_CALL(*cli, getRepo(::testing::_))
      .WillOnce(::testing::Return(makeGetRepoError("no repo")));
    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->size(options), -1);
}

TEST_F(CliTest, dependsReportsRepoError)
{
    cli::DependsOptions options;
    options.appid = "main:org.example/1.0.0/x86_64";

    EXPECT_CALL(*cli, getRepo(::testing::_))
      .WillOnce(::testing::Return(makeGetRepoError("no repo")));
    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->depends(options), -1);
}

TEST_F(CliTest, dependsRejectsInvalidAppid)
{
    cli::DependsOptions options;
    options.appid = "org.example/1.0.0/x86_64/extra";

    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->depends(options), -1);
}

TEST_F(CliTest, dependsReportsLocalResolveError)
{
    cli::DependsOptions options;
    options.appid = "main:org.example/1.0.0/x86_64";

    EXPECT_CALL(*repo, clearReferenceLocal(::testing::_, ::testing::_))
      .WillOnce(::testing::Return(makeRepoReferenceError("not installed")));
    EXPECT_CALL(*printer, printErr(::testing::_)).Times(1);
    EXPECT_EQ(cli->depends(options), -1);
}

TEST_F(CliTest, listPrintsImportedLayers)
{
    ASSERT_TRUE(repo->initialize());

    auto appLayer = importLayer(*repo,
                                tempDir->path() / "layer-sources",
                                "org.example.app",
                                "1.0.0",
                                "app",
                                "binary");
    ASSERT_TRUE(appLayer);

    std::vector<api::types::v1::PackageInfoDisplay> printed;
    EXPECT_CALL(*printer, printPackages(_))
      .WillOnce(Invoke([&printed](const std::vector<api::types::v1::PackageInfoDisplay> &packages) {
          printed = packages;
      }));

    EXPECT_EQ(cli->list(cli::ListOptions{}), 0);

    ASSERT_EQ(printed.size(), 1U);
    EXPECT_EQ(printed.front().id, "org.example.app");
    EXPECT_EQ(printed.front().version, "1.0.0");
    EXPECT_EQ(printed.front().packageInfoDisplayModule, "binary");
    EXPECT_EQ(printed.front().kind, "app");
}

TEST_F(CliTest, listFiltersLayersByRequestedType)
{
    ASSERT_TRUE(repo->initialize());

    ASSERT_TRUE(importLayer(*repo,
                            tempDir->path() / "layer-sources",
                            "org.example.app",
                            "1.0.0",
                            "app",
                            "binary"));
    ASSERT_TRUE(importLayer(*repo,
                            tempDir->path() / "layer-sources",
                            "org.example.runtime",
                            "1.0.0",
                            "runtime",
                            "runtime"));

    std::vector<api::types::v1::PackageInfoDisplay> printed;
    EXPECT_CALL(*printer, printPackages(_))
      .WillOnce(Invoke([&printed](const std::vector<api::types::v1::PackageInfoDisplay> &packages) {
          printed = packages;
      }));

    cli::ListOptions options;
    options.type = "runtime";
    EXPECT_EQ(cli->list(options), 0);
    ASSERT_EQ(printed.size(), 1U);
    EXPECT_EQ(printed.front().id, "org.example.runtime");
}

TEST_F(CliTest, infoPrintsImportedLayerInfo)
{
    ASSERT_TRUE(repo->initialize());

    auto appLayer = importLayer(*repo,
                                tempDir->path() / "layer-sources",
                                "org.example.app",
                                "1.0.0",
                                "app",
                                "binary");
    ASSERT_TRUE(appLayer);

    auto fuzzyRef = package::FuzzyReference::parse("stable:org.example.app/1.0.0/x86_64");
    ASSERT_TRUE(fuzzyRef);
    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(appLayer->reference));
    EXPECT_CALL(*repo, getLayerItem(_, _))
      .WillRepeatedly(Invoke([item = appLayer->item](const package::Reference &, std::string)
                               -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
          return item;
      }));

    api::types::v1::PackageInfoV2 printed;
    EXPECT_CALL(*printer, printPackage(_))
      .WillOnce(Invoke([&printed](const api::types::v1::PackageInfoV2 &info) {
          printed = info;
      }));

    cli::InfoOptions options;
    options.appid = "stable:org.example.app/1.0.0/x86_64";
    EXPECT_EQ(cli->info(options), 0);
    EXPECT_EQ(printed.id, "org.example.app");
    EXPECT_EQ(printed.version, "1.0.0");
    EXPECT_EQ(printed.packageInfoV2Module, "binary");
}

TEST_F(CliTest, infoReportsMissingLocalReference)
{
    EXPECT_CALL(*repo, clearReferenceLocal(_, _))
      .WillOnce(Return(makeRepoReferenceError("not installed")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    cli::InfoOptions options;
    options.appid = "stable:org.example.app/1.0.0/x86_64";
    EXPECT_EQ(cli->info(options), -1);
}

TEST_F(CliTest, sizeReportsImportedModuleSizes)
{
    ASSERT_TRUE(repo->initialize());

    ASSERT_TRUE(importLayer(*repo,
                            tempDir->path() / "layer-sources",
                            "org.example.app",
                            "1.0.0",
                            "app",
                            "binary"));

    std::vector<cli::Printer::ModuleSizeInfo> printed;
    EXPECT_CALL(*printer, printModuleSizes(_, _, _))
      .WillOnce(Invoke([&printed](const std::vector<cli::Printer::ModuleSizeInfo> &sizes,
                                  std::uint64_t,
                                  std::uint64_t) {
          printed = sizes;
      }));

    EXPECT_EQ(cli->size(cli::SizeOptions{}), 0);
    ASSERT_EQ(printed.size(), 1U);
    EXPECT_EQ(printed.front().id, "org.example.app");
    EXPECT_EQ(printed.front().module, "binary");
}

TEST_F(CliTest, dependsPrintsResolvedAppHierarchy)
{
    ASSERT_TRUE(repo->initialize());

    const auto workspace = tempDir->path() / "layer-sources";
    auto appLayer = importLayer(*repo,
                                workspace,
                                "org.example.app",
                                "1.0.0",
                                "app",
                                "binary",
                                "org.deepin.base/23.0.0",
                                "org.deepin.runtime/23.0.0");
    auto baseLayer = importLayer(*repo, workspace, "org.deepin.base", "23.0.0", "base", "binary");
    auto runtimeLayer =
      importLayer(*repo, workspace, "org.deepin.runtime", "23.0.0", "runtime", "runtime");
    ASSERT_TRUE(appLayer);
    ASSERT_TRUE(baseLayer);
    ASSERT_TRUE(runtimeLayer);

    auto resolve = [&](const package::FuzzyReference &fuzzy,
                       bool) -> utils::error::Result<package::Reference> {
        if (fuzzy.id == "org.example.app") {
            return appLayer->reference;
        }
        if (fuzzy.id == "org.deepin.base") {
            return baseLayer->reference;
        }
        if (fuzzy.id == "org.deepin.runtime") {
            return runtimeLayer->reference;
        }
        LINGLONG_TRACE("clearReferenceLocal");
        return LINGLONG_ERR("unexpected reference");
    };
    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillRepeatedly(Invoke(resolve));

    auto layerItem =
      [&](const package::Reference &ref,
          std::string) -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
        if (ref.id == "org.example.app") {
            return appLayer->item;
        }
        if (ref.id == "org.deepin.base") {
            return baseLayer->item;
        }
        if (ref.id == "org.deepin.runtime") {
            return runtimeLayer->item;
        }
        LINGLONG_TRACE("getLayerItem");
        return LINGLONG_ERR("unexpected layer");
    };
    EXPECT_CALL(*repo, getLayerItem(_, _)).WillRepeatedly(Invoke(layerItem));

    std::vector<cli::Printer::DependsNode> printed;
    EXPECT_CALL(*printer, printDepends(_))
      .WillOnce(Invoke([&printed](const std::vector<cli::Printer::DependsNode> &depends) {
          printed = depends;
      }));

    cli::DependsOptions options;
    options.appid = "stable:org.example.app/1.0.0/x86_64";
    EXPECT_EQ(cli->depends(options), 0);
    ASSERT_EQ(printed.size(), 1U);
    EXPECT_THAT(printed.front().ref, HasSubstr("org.deepin.base"));
    ASSERT_EQ(printed.front().children.size(), 1U);
    EXPECT_THAT(printed.front().children.front().ref, HasSubstr("org.deepin.runtime"));
    ASSERT_EQ(printed.front().children.front().children.size(), 1U);
    EXPECT_THAT(printed.front().children.front().children.front().ref,
                HasSubstr("org.example.app"));
}

TEST_F(CliTest, dependsRejectsNonAppTarget)
{
    cli::DependsOptions options;
    options.appid = "stable:org.example.runtime/1.0.0/x86_64";

    auto ref = package::Reference::parse("stable:org.example.runtime/1.0.0/x86_64");
    ASSERT_TRUE(ref);

    api::types::v1::RepositoryCacheLayersItem item;
    item.info.kind = "runtime";

    EXPECT_CALL(*repo, clearReferenceLocal(_, _)).WillOnce(Return(*ref));
    EXPECT_CALL(*repo, getLayerItem(_, _)).WillOnce(Return(item));
    EXPECT_CALL(*printer, printErr(_)).Times(1);
    EXPECT_EQ(cli->depends(options), -1);
}

TEST_F(CliTest, killReportsContainerListFailure)
{
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    cli::KillOptions options;
    options.appid = "org.example.app";
    EXPECT_EQ(cli->kill(options), -1);
}

TEST_F(CliTest, enterReportsContainerListFailure)
{
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    cli::EnterOptions options;
    options.instance = "org.example.app";
    EXPECT_EQ(cli->enter(options), -1);
}

TEST_F(CliTest, installReportsPackageManagerError)
{
    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Return(makePackageManagerError("no package manager")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    cli::InstallOptions options;
    options.appid = "stable:org.example.app/1.0.0/x86_64";
    EXPECT_EQ(cli->install(options), -1);
}

TEST_F(CliTest, uninstallReportsPackageManagerError)
{
    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Return(makePackageManagerError("no package manager")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    cli::UninstallOptions options;
    options.appid = "stable:org.example.app/1.0.0/x86_64";
    EXPECT_EQ(cli->uninstall(options), -1);
}

TEST_F(CliTest, pruneReportsPackageManagerError)
{
    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Return(makePackageManagerError("no package manager")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    EXPECT_EQ(cli->prune(), -1);
}

TEST_F(CliTest, searchReportsPackageManagerError)
{
    repo = std::make_unique<MockRepo>(
      tempDir->path(),
      api::types::v1::RepoConfigV2{
        .defaultRepo = "stable",
        .repos = { api::types::v1::Repo{ .name = "stable",
                                         .priority = 0,
                                         .url = "https://example.com/repo" } },
        .version = 2,
      });

    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Return(makePackageManagerError("no package manager")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    EXPECT_EQ(cli->search(cli::SearchOptions{}), -1);
}

TEST_F(CliTest, runWithContextRejectsMissingContext)
{
    EXPECT_CALL(*printer, printErr(_)).Times(1);
    EXPECT_EQ(cli->runWithContext(cli::RunOptions{}), -1);
}

TEST_F(CliTest, runWithContextRejectsMalformedContext)
{
    cli::RunOptions options;
    options.appid = "org.example.app";
    options.runContext = std::string{ "not-a-json-object" };

    EXPECT_CALL(*printer, printErr(_)).Times(1);
    EXPECT_EQ(cli->runWithContext(options), -1);
}

TEST_F(CliTest, infoRejectsEmptyAppid)
{
    EXPECT_CALL(*printer, printErr(_)).Times(1);
    EXPECT_EQ(cli->info(cli::InfoOptions{}), -1);
}

TEST_F(CliTest, upgradeWithoutAppidReportsPackageManagerError)
{
    EXPECT_CALL(*cli, getPkgMan()).WillOnce(Return(makePackageManagerError("no package manager")));
    EXPECT_CALL(*printer, printErr(_)).Times(1);

    EXPECT_EQ(cli->upgrade(cli::UpgradeOptions{}), -1);
}

} // namespace

namespace {

constexpr auto runtimeContainerID = "abcdef0123456789abcdef0123456789";
constexpr auto runtimePackage = "stable:org.example.app/1.0.0/x86_64";
constexpr auto runtimeInfoRoot = "/run/linglong";

std::filesystem::path currentRuntimeInfoDir()
{
    return std::filesystem::path{ runtimeInfoRoot } / std::to_string(::getuid());
}

class ScopedRuntimeInfo
{
public:
    explicit ScopedRuntimeInfo(std::initializer_list<std::pair<pid_t, std::string>> entries)
    {
        infoDir = currentRuntimeInfoDir();
        std::error_code ec;
        std::filesystem::create_directories(infoDir, ec);
        if (ec) {
            failure = fmt::format("failed to create {}: {}", infoDir.string(), ec.message());
            return;
        }

        for (const auto &[pid, containerID] : entries) {
            auto file = infoDir / std::to_string(pid);
            nlohmann::json content{ { "app", runtimePackage },
                                    { "base", "" },
                                    { "containerID", containerID } };
            std::ofstream out{ file };
            out << content.dump();
            out.flush();
            files.push_back(file);
            if (!out) {
                failure = fmt::format("failed to write {}", file.string());
                return;
            }
        }
    }

    ~ScopedRuntimeInfo()
    {
        std::error_code ec;
        for (const auto &file : files) {
            std::filesystem::remove(file, ec);
        }
    }

    [[nodiscard]] const std::string &error() const noexcept { return failure; }

private:
    std::filesystem::path infoDir;
    std::vector<std::filesystem::path> files;
    std::string failure;
};

class RuntimeCliTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        printer = std::make_unique<MockPrinter>();

        runtimeInfoDir = currentRuntimeInfoDir();
        std::error_code ec;
        runtimeInfoDirExisted = std::filesystem::exists(runtimeInfoDir, ec);
        if (ec) {
            GTEST_SKIP() << "cannot inspect " << runtimeInfoDir << ": " << ec.message();
        }
        runtimeInfoRootExisted = std::filesystem::exists(runtimeInfoRoot, ec);
        if (ec) {
            GTEST_SKIP() << "cannot inspect " << runtimeInfoRoot << ": " << ec.message();
        }

        std::filesystem::create_directories(runtimeInfoDir, ec);
        if (ec) {
            GTEST_SKIP() << "runtime info directory " << runtimeInfoDir
                         << " is not usable: " << ec.message();
        }

        auto probe = runtimeInfoDir / fmt::format(".ll-tests-probe-{}", ::getpid());
        {
            std::ofstream out{ probe };
            if (!out) {
                GTEST_SKIP() << "runtime info directory " << runtimeInfoDir << " is not writable";
            }
        }
        std::filesystem::remove(probe, ec);
    }

    void TearDown() override
    {
        std::error_code ec;
        if (!runtimeInfoDirExisted) {
            std::filesystem::remove(runtimeInfoDir, ec);
        }
        if (!runtimeInfoRootExisted) {
            std::filesystem::remove(runtimeInfoRoot, ec);
        }
    }

    void writeRuntime(const std::string &json)
    {
        runtimePath = tempDir->path() / "crun";
        invocationLog = tempDir->path() / "runtime-invocations.log";
        {
            std::ofstream log{ invocationLog };
            ASSERT_TRUE(log) << "failed to create " << invocationLog;
        }
        std::ofstream runtime{ runtimePath };
        runtime << "#!/bin/sh\n"
                   "for arg in \"$@\"; do\n"
                   "  if [ \"$arg\" = \"list\" ]; then\n"
                   "    cat <<'JSON'\n"
                << json
                << "\nJSON\n"
                   "    exit 0\n"
                   "  fi\n"
                   "  if [ \"$arg\" = \"kill\" ] || [ \"$arg\" = \"exec\" ]; then\n"
                   "    printf '%s\\n' \"$*\" >> '"
                << invocationLog.string()
                << "'\n"
                   "    exit 0\n"
                   "  fi\n"
                   "done\n"
                   "exit 1\n";
        std::filesystem::permissions(runtimePath,
                                     std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
        auto created = ocppi::cli::crun::Crun::New(runtimePath);
        ASSERT_TRUE(created.has_value());
        ociCLI = std::move(*created);
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        auto notifier = std::make_unique<cli::DummyNotifier>();
        cli = std::make_unique<::testing::NiceMock<MockCli>>(*printer,
                                                             *ociCLI,
                                                             *containerBuilder,
                                                             false,
                                                             std::move(notifier),
                                                             nullptr);
    }

    [[nodiscard]] std::vector<std::string> readInvocations() const
    {
        std::ifstream input{ invocationLog };
        std::vector<std::string> lines;
        for (std::string line; std::getline(input, line);) {
            lines.push_back(std::move(line));
        }
        return lines;
    }

    std::unique_ptr<TempDir> tempDir;
    std::filesystem::path runtimePath;
    std::filesystem::path invocationLog;
    std::filesystem::path runtimeInfoDir;
    bool runtimeInfoDirExisted{ false };
    bool runtimeInfoRootExisted{ false };
    std::unique_ptr<MockPrinter> printer;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<::testing::NiceMock<MockCli>> cli;
};

TEST_F(RuntimeCliTest, PsPrintsAndTruncatesRunningContainers)
{
    ScopedRuntimeInfo info{ { ::getpid(), runtimeContainerID } };
    ASSERT_TRUE(info.error().empty()) << info.error();
    writeRuntime(fmt::format(
      R"([{{"bundle":"/tmp","created":"","id":"{}","owner":"0","pid":{},"status":"running"}}])",
      runtimeContainerID,
      ::getpid()));

    std::vector<std::vector<api::types::v1::CliContainer>> printed;
    EXPECT_CALL(*printer, printContainers(_))
      .Times(2)
      .WillRepeatedly([&printed](const std::vector<api::types::v1::CliContainer> &containers) {
          printed.push_back(containers);
      });

    EXPECT_EQ(cli->ps(cli::PsOptions{}), 0);
    EXPECT_EQ(cli->ps(cli::PsOptions{ .noTruncate = true }), 0);

    ASSERT_EQ(printed.size(), 2);
    ASSERT_EQ(printed[0].size(), 1);
    EXPECT_EQ(printed[0].front().id, std::string{ runtimeContainerID }.substr(0, 12));
    EXPECT_EQ(printed[0].front().package, runtimePackage);
    EXPECT_EQ(printed[1].front().id, runtimeContainerID);
}

TEST_F(RuntimeCliTest, KillMatchesPackageAndSignalsContainer)
{
    ScopedRuntimeInfo info{ { ::getpid(), runtimeContainerID } };
    ASSERT_TRUE(info.error().empty()) << info.error();
    writeRuntime(fmt::format(
      R"([{{"bundle":"/tmp","created":"","id":"{}","owner":"0","pid":{},"status":"running"}}])",
      runtimeContainerID,
      ::getpid()));

    EXPECT_EQ(cli->kill(cli::KillOptions{ .appid = "org.example.app" }), 0);

    auto invocations = readInvocations();
    ASSERT_EQ(invocations.size(), 1);
    EXPECT_THAT(invocations.front(), HasSubstr("kill"));
    EXPECT_THAT(invocations.front(), HasSubstr(runtimeContainerID));
    EXPECT_THAT(invocations.front(), HasSubstr("SIGTERM"));
}

TEST_F(RuntimeCliTest, KillReportsMissingAndAmbiguousContainers)
{
    {
        ScopedRuntimeInfo empty({});
        ASSERT_TRUE(empty.error().empty()) << empty.error();
        writeRuntime(fmt::format(
          R"([{{"bundle":"/tmp","created":"","id":"{}","owner":"0","pid":{},"status":"running"}}])",
          runtimeContainerID,
          ::getpid()));
        EXPECT_CALL(*printer, printErr(_)).Times(1);
        EXPECT_EQ(cli->kill(cli::KillOptions{ .appid = "org.example.app" }), -1);
    }

    ScopedRuntimeInfo info{ { ::getpid(), runtimeContainerID },
                            { ::getppid(), "0123456789abcdef0123456789abcdef" } };
    ASSERT_TRUE(info.error().empty()) << info.error();
    writeRuntime(fmt::format(
      R"([{{"bundle":"/tmp","created":"","id":"{}","owner":"0","pid":{},"status":"running"}},{{"bundle":"/tmp","created":"","id":"0123456789abcdef0123456789abcdef","owner":"0","pid":{},"status":"running"}}])",
      runtimeContainerID,
      ::getpid(),
      ::getppid()));
    EXPECT_CALL(*printer, printErr(_)).Times(1);
    EXPECT_EQ(cli->kill(cli::KillOptions{ .appid = "org.example.app" }), -1);
}

TEST_F(RuntimeCliTest, EnterExecsCommandInMatchedContainer)
{
    ScopedRuntimeInfo info{ { ::getpid(), runtimeContainerID } };
    ASSERT_TRUE(info.error().empty()) << info.error();
    writeRuntime(fmt::format(
      R"([{{"bundle":"/tmp","created":"","id":"{}","owner":"0","pid":{},"status":"running"}}])",
      runtimeContainerID,
      ::getpid()));

    EXPECT_EQ(cli->enter(cli::EnterOptions{ .instance = runtimeContainerID,
                                            .commands = { "echo", "hello" } }),
              0);

    auto invocations = readInvocations();
    ASSERT_EQ(invocations.size(), 1);
    EXPECT_THAT(invocations.front(), HasSubstr("exec"));
    EXPECT_THAT(invocations.front(), HasSubstr(runtimeContainerID));
    EXPECT_THAT(invocations.front(), HasSubstr("echo"));
    EXPECT_THAT(invocations.front(), HasSubstr("hello"));
}

} // namespace
