/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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
            QDir(path.c_str()),
            api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>,
                listLocal,
                (),
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
        pkgMan =
          std::make_unique<api::dbus::v1::PackageManager>("",
                                                          "/org/deepin/linglong/PackageManager1",
                                                          QDBusConnection::sessionBus(),
                                                          nullptr);
        repo = std::make_unique<MockRepo>(tempDir->path());
        auto notifier = std::make_unique<cli::DummyNotifier>();
        cli = std::make_unique<cli::Cli>(*printer,
                                         *ociCLI,
                                         *containerBuilder,
                                         *pkgMan,
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
        pkgMan.reset();
        repo.reset();
        cli.reset();
    }

    std::unique_ptr<MockPrinter> printer;
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<api::dbus::v1::PackageManager> pkgMan;
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

} // namespace
