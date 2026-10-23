/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#define private public
#include "linglong/cli/cli.h"
#undef private
#include "linglong/cli/dummy_notifier.h"
#include "linglong/utils/env.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

using namespace linglong;
using ::testing::ElementsAre;

namespace {

class MockPrinter : public cli::Printer
{
public:
    MOCK_METHOD(void, printErr, (const utils::error::Error &error), (override));
    MOCK_METHOD(void, printPackage, (const api::types::v1::PackageInfoV2 &), (override));
    MOCK_METHOD(void,
                printPackages,
                (const std::vector<api::types::v1::PackageInfoDisplay> &),
                (override));
    MOCK_METHOD(void,
                printSearchResult,
                ((std::map<std::string, std::vector<api::types::v1::PackageInfoV2>>)),
                (override));
    MOCK_METHOD(void,
                printPruneResult,
                (const std::vector<api::types::v1::PackageInfoV2> &),
                (override));
    MOCK_METHOD(void,
                printContainers,
                (const std::vector<api::types::v1::CliContainer> &),
                (override));
    MOCK_METHOD(void, printRepoConfig, (const api::types::v1::RepoConfigV2 &), (override));
    MOCK_METHOD(void, printLayerInfo, (const api::types::v1::LayerInfo &), (override));
    MOCK_METHOD(void, printProgress, (double, const std::string &), (override));
    MOCK_METHOD(void, printContent, (const QStringList &), (override));
    MOCK_METHOD(void,
                printUpgradeList,
                (std::vector<api::types::v1::UpgradeListResult> &),
                (override));
    MOCK_METHOD(void, printInspect, (const api::types::v1::InspectResult &), (override));
    MOCK_METHOD(void,
                printModuleSizes,
                (const std::vector<cli::Printer::ModuleSizeInfo> &, std::uint64_t, std::uint64_t),
                (override));
    MOCK_METHOD(void, printDepends, (const std::vector<cli::Printer::DependsNode> &), (override));
    MOCK_METHOD(void, printMessage, (const std::string &), (override));
    MOCK_METHOD(void, finishProgress, (), (override));
};

class CliHelperTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        ociCLI = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        cli = std::make_unique<cli::Cli>(*printer,
                                         *ociCLI,
                                         *containerBuilder,
                                         false,
                                         std::make_unique<cli::DummyNotifier>(),
                                         nullptr);
    }

    void TearDown() override
    {
        cli.reset();
        containerBuilder.reset();
        ociCLI.reset();
        tempDir.reset();
    }

    std::shared_ptr<::testing::NiceMock<MockPrinter>> printer =
      std::make_shared<::testing::NiceMock<MockPrinter>>();
    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    std::unique_ptr<cli::Cli> cli;
};

api::types::v1::PackageInfoV2 makePkg(const std::string &id,
                                      const std::string &version,
                                      const std::string &kind = "app",
                                      const std::string &module = "binary")
{
    api::types::v1::PackageInfoV2 pkg;
    pkg.id = id;
    pkg.version = version;
    pkg.kind = kind;
    pkg.packageInfoV2Module = module;
    return pkg;
}

TEST(CliStaticHelpers, MappingFileMapsAbsolutePathToRunHostRootfs)
{
    const std::filesystem::path file = "/etc/hostname";

    auto mapped = cli::Cli::mappingFile(file);

    EXPECT_EQ(mapped, "/run/host/rootfs/etc/hostname");
}

TEST(CliStaticHelpers, MappingFileLeavesRelativePathUnchanged)
{
    const std::filesystem::path file = "relative/path.txt";

    auto mapped = cli::Cli::mappingFile(file);

    EXPECT_EQ(mapped, "relative/path.txt");
}

TEST(CliStaticHelpers, MappingFileLeavesHomeFilesUnchanged)
{
    utils::EnvironmentVariableGuard env("HOME", "/home/test-user");

    const std::filesystem::path file = "/home/test-user/documents/file.txt";

    auto mapped = cli::Cli::mappingFile(file);

    EXPECT_EQ(mapped, "/home/test-user/documents/file.txt");
}

TEST(CliStaticHelpers, MappingFileResolvesSymlinkBeforeMapping)
{
    TempDir dir;

    const auto target = dir.path() / "target.txt";
    std::ofstream(target).put('x');
    const auto link = dir.path() / "link.txt";
    std::filesystem::create_symlink(target, link);

    auto mapped = cli::Cli::mappingFile(link);

    EXPECT_THAT(mapped, ::testing::StartsWith("/run/host/rootfs/"));
    EXPECT_THAT(mapped, ::testing::EndsWith("/target.txt"));
}

TEST(CliStaticHelpers, MappingUrlKeepsRemoteUrlUnchanged)
{
    const std::string url = "https://example.com/file.txt";

    auto mapped = cli::Cli::mappingUrl(url);

    EXPECT_EQ(mapped, url);
}

TEST(CliStaticHelpers, MappingUrlMapsLeadingSlashPath)
{
    const std::string url = "/etc/hostname";

    auto mapped = cli::Cli::mappingUrl(url);

    EXPECT_EQ(mapped, "/run/host/rootfs/etc/hostname");
}

TEST(CliStaticHelpers, MappingUrlMapsFileScheme)
{
    const std::string url = "file:///etc/hostname";

    auto mapped = cli::Cli::mappingUrl(url);

    EXPECT_EQ(mapped, "file:///run/host/rootfs/etc/hostname");
}

TEST_F(CliHelperTest, FilePathMappingKeepsPlainArguments)
{
    cli::RunOptions options;
    std::vector<std::string> command = { "/bin/echo", "hello", "world" };

    auto result = cli->filePathMapping(command, options);

    EXPECT_EQ(result, command);
}

TEST_F(CliHelperTest, FilePathMappingExpandsSingleFile)
{
    cli::RunOptions options;
    options.filePaths = { "/etc/hostname" };
    std::vector<std::string> command = { "app", "%f" };

    auto result = cli->filePathMapping(command, options);

    EXPECT_EQ(result, (std::vector<std::string>{ "app", "/run/host/rootfs/etc/hostname" }));
}

TEST_F(CliHelperTest, FilePathMappingExpandsFileListAndSkipsEmpty)
{
    cli::RunOptions options;
    options.filePaths = { "/etc/hostname", "", "/etc/hosts" };
    std::vector<std::string> command = { "app", "%F" };

    auto result = cli->filePathMapping(command, options);

    EXPECT_EQ(result,
              (std::vector<std::string>{
                "app",
                "/run/host/rootfs/etc/hostname",
                "/run/host/rootfs/etc/hosts",
              }));
}

TEST_F(CliHelperTest, FilePathMappingExpandsUrls)
{
    cli::RunOptions options;
    options.fileUrls = { "https://example.com/a", "/etc/hostname" };
    std::vector<std::string> command = { "app", "%U" };

    auto result = cli->filePathMapping(command, options);

    EXPECT_EQ(result,
              (std::vector<std::string>{ "app",
                                         "https://example.com/a",
                                         "/run/host/rootfs/etc/hostname" }));
}

TEST_F(CliHelperTest, FilePathMappingDropsUnknownPercentArgument)
{
    cli::RunOptions options;
    std::vector<std::string> command = { "app", "%x", "tail" };

    auto result = cli->filePathMapping(command, options);

    EXPECT_EQ(result, (std::vector<std::string>{ "app", "tail" }));
}

TEST(CliStaticHelpers, FilterPackageInfosByTypeAllKeepsEverything)
{
    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> list = {
        { "stable", { makePkg("org.example.app", "1.0.0", "app") } },
        { "runtime", { makePkg("org.example.lib", "1.0.0", "runtime") } },
    };

    cli::Cli::filterPackageInfosByType(list, "all");

    EXPECT_EQ(list.size(), 2u);
}

TEST(CliStaticHelpers, FilterPackageInfosByTypeFiltersAndDropsEmptyGroups)
{
    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> list = {
        { "stable", { makePkg("org.example.app", "1.0.0", "app") } },
        { "runtime", { makePkg("org.example.lib", "1.0.0", "runtime") } },
    };

    cli::Cli::filterPackageInfosByType(list, "app");

    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list.begin()->first, "stable");
    EXPECT_EQ(list.begin()->second.at(0).id, "org.example.app");
}

TEST(CliStaticHelpers, FilterPackageInfosByTypeDisplayFilters)
{
    api::types::v1::PackageInfoDisplay app;
    app.id = "org.example.app";
    app.kind = "app";
    api::types::v1::PackageInfoDisplay runtime;
    runtime.id = "org.example.lib";
    runtime.kind = "runtime";

    std::vector<api::types::v1::PackageInfoDisplay> list = { app, runtime };

    cli::Cli::filterPackageInfosByType(list, "app");

    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list.at(0).id, "org.example.app");
}

TEST(CliStaticHelpers, FilterPackageInfosByVersionKeepsHighest)
{
    std::map<std::string, std::vector<api::types::v1::PackageInfoV2>> list = {
        { "stable",
          { makePkg("org.example.app", "1.0.0"),
            makePkg("org.example.app", "2.1.0"),
            makePkg("org.example.app", "1.5.0") } },
    };

    cli::Cli::filterPackageInfosByVersion(list);

    ASSERT_EQ(list.at("stable").size(), 1u);
    EXPECT_EQ(list.at("stable").at(0).version, "2.1.0");
}

TEST_F(CliHelperTest, IsContainerIDMatchMatchesExactId)
{
    EXPECT_TRUE(cli->isContainerIDMatch("abcdef1234567890", "abcdef1234567890"));
}

TEST_F(CliHelperTest, IsContainerIDMatchMatchesShortPrefix)
{
    EXPECT_TRUE(cli->isContainerIDMatch("abcdef1234567890", "abcdef123456"));
}

TEST_F(CliHelperTest, IsContainerIDMatchRejectsTooShortPrefix)
{
    EXPECT_FALSE(cli->isContainerIDMatch("abcdef1234567890", "abc"));
}

TEST_F(CliHelperTest, IsContainerIDMatchRejectsDifferentId)
{
    EXPECT_FALSE(cli->isContainerIDMatch("abcdef1234567890", "fedcba123456"));
}

} // namespace
