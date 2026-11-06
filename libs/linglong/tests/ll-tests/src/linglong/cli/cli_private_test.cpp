/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/dummy_notifier.h"
#define private public
#include "linglong/cli/cli.h"
#undef private
#include "linglong/cli/cli_printer.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

using namespace linglong;
namespace v1 = api::types::v1;

using ErrorCode = utils::error::ErrorCode;

utils::error::Error makeError(ErrorCode code, const std::string &message = "error")
{
    return utils::error::Error::Err(__FILE__, __LINE__, "cli private test", message, code);
}

v1::PackageInfoV2 makePackage(const std::string &id,
                              const std::string &version,
                              const std::string &kind = "app",
                              const std::string &module = "binary")
{
    return v1::PackageInfoV2{
        .arch = { "x86_64" },
        .channel = "main",
        .id = id,
        .kind = kind,
        .packageInfoV2Module = module,
        .version = version,
    };
}

class RecordingPrinter : public cli::CLIPrinter
{
public:
    void printErr(const utils::error::Error &error) override { errors.push_back(error.message()); }

    void printMessage(const std::string &message) override { messages.push_back(message); }

    void printSearchResult(std::map<std::string, std::vector<v1::PackageInfoV2>> packages) override
    {
        searchResults.push_back(std::move(packages));
    }

    std::vector<std::string> messages;
    std::vector<std::string> errors;
    std::vector<std::map<std::string, std::vector<v1::PackageInfoV2>>> searchResults;
};

class CliPrivateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        auto createdCli = ocppi::cli::crun::Crun::New("/bin/false");
        ASSERT_TRUE(createdCli.has_value());
        ociCLI = std::move(*createdCli);
        containerBuilder = std::make_unique<runtime::ContainerBuilder>(*ociCLI);
        cli = std::make_unique<cli::Cli>(printer,
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

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> ociCLI;
    std::unique_ptr<runtime::ContainerBuilder> containerBuilder;
    RecordingPrinter printer;
    std::unique_ptr<cli::Cli> cli;
};

TEST_F(CliPrivateTest, FilePathMappingExpandsDesktopPlaceholders)
{
    const auto home = tempDir->path();
    auto file = home / "document.txt";
    auto linkedFile = home / "linked.txt";
    std::ofstream{ file } << "payload";
    std::filesystem::create_symlink(file, linkedFile);

    const auto oldHome = std::string{ std::getenv("HOME") == nullptr ? "" : std::getenv("HOME") };

    struct HomeGuard
    {
        std::string old;

        ~HomeGuard()
        {
            if (old.empty()) {
                ::unsetenv("HOME");
            } else {
                ::setenv("HOME", old.c_str(), 1);
            }
        }
    } homeGuard{ oldHome };

    ::setenv("HOME", home.c_str(), 1);

    cli::RunOptions options;
    options.filePaths = { file.string(), linkedFile.string(), "" };
    options.fileUrls = { "https://example.com/document.txt",
                         "file://" + file.string(),
                         file.string(),
                         "" };

    auto args = cli->filePathMapping({ "application", "%f", "%F", "%u", "%U", "%d" }, options);

    ASSERT_EQ(args.size(), 11U);
    EXPECT_EQ(args[0], "application");
    EXPECT_EQ(args[1], file.string());
    EXPECT_EQ(args[2], file.string());
    EXPECT_EQ(args[3], file.string());
    EXPECT_EQ(args[4], file.string());
    EXPECT_EQ(args[5], "https://example.com/document.txt");
    EXPECT_EQ(args[6], "file://" + file.string());
    EXPECT_EQ(args[7], file.string());
    EXPECT_EQ(args[8], "https://example.com/document.txt");
    EXPECT_EQ(args[9], "file://" + file.string());
    EXPECT_EQ(args[10], file.string());
}

TEST_F(CliPrivateTest, FileMappingLeavesRelativePathsAndUnknownUrlsUntouched)
{
    EXPECT_EQ(cli::Cli::mappingFile("relative/file.txt"), "relative/file.txt");
    EXPECT_EQ(cli::Cli::mappingUrl("https://example.com/a"), "https://example.com/a");
    EXPECT_EQ(cli::Cli::mappingUrl("mailto:user@example.com"), "mailto:user@example.com");
}

TEST_F(CliPrivateTest, PackageFiltersSelectKindAndLatestVersion)
{
    std::map<std::string, std::vector<v1::PackageInfoV2>> byType{
        { "stable",
          { makePackage("org.example.app", "1.0.0", "app"),
            makePackage("org.example.base", "1.0.0", "base") } },
        { "testing", { makePackage("org.example.base", "2.0.0", "base") } },
    };

    cli->filterPackageInfosByType(byType, "app");
    ASSERT_EQ(byType.size(), 1U);
    ASSERT_EQ(byType["stable"].size(), 1U);
    EXPECT_EQ(byType["stable"].front().id, "org.example.app");

    cli->filterPackageInfosByType(byType, "all");
    EXPECT_EQ(byType.size(), 1U);

    std::map<std::string, std::vector<v1::PackageInfoV2>> versions{
        { "stable",
          { makePackage("org.example.app", "1.0.0"),
            makePackage("org.example.app", "2.0.0"),
            makePackage("org.example.app", "1.5.0"),
            makePackage("org.example.bad-new", "1.0.0"),
            makePackage("org.example.bad-new", ""),
            makePackage("org.example.bad-old", ""),
            makePackage("org.example.bad-old", "2.0.0"),
            makePackage("org.example.module", "1.0.0", "app", "binary"),
            makePackage("org.example.module", "3.0.0", "app", "develop") } },
    };

    cli->filterPackageInfosByVersion(versions);

    ASSERT_EQ(versions["stable"].size(), 5U);
    auto find = [&versions](const std::string &id, const std::string &module = "binary") {
        return std::find_if(versions["stable"].begin(),
                            versions["stable"].end(),
                            [&id, &module](const auto &pkg) {
                                return pkg.id == id && pkg.packageInfoV2Module == module;
                            });
    };
    auto app = find("org.example.app");
    ASSERT_NE(app, versions["stable"].end());
    EXPECT_EQ(app->version, "2.0.0");
    auto badNew = find("org.example.bad-new");
    ASSERT_NE(badNew, versions["stable"].end());
    EXPECT_EQ(badNew->version, "1.0.0");
    auto badOld = find("org.example.bad-old");
    ASSERT_NE(badOld, versions["stable"].end());
    EXPECT_EQ(badOld->version, "");
    auto develop = find("org.example.module", "develop");
    ASSERT_NE(develop, versions["stable"].end());
    EXPECT_EQ(develop->version, "3.0.0");

    std::vector<v1::PackageInfoDisplay> display{
        v1::PackageInfoDisplay{ .arch = { "x86_64" },
                                .base = "",
                                .channel = "main",
                                .id = "org.example.app",
                                .kind = "app",
                                .packageInfoDisplayModule = "binary",
                                .name = "App",
                                .schemaVersion = "1.0",
                                .size = 1,
                                .version = "1.0.0" },
        v1::PackageInfoDisplay{ .arch = { "x86_64" },
                                .base = "",
                                .channel = "main",
                                .id = "org.example.base",
                                .kind = "base",
                                .packageInfoDisplayModule = "binary",
                                .name = "Base",
                                .schemaVersion = "1.0",
                                .size = 1,
                                .version = "1.0.0" },
    };
    cli->filterPackageInfosByType(display, "app");
    ASSERT_EQ(display.size(), 1U);
    EXPECT_EQ(display.front().id, "org.example.app");
}

TEST_F(CliPrivateTest, CommonErrorsMapToUserFacingMessages)
{
    for (const auto code : { ErrorCode::NetworkError,
                             ErrorCode::LayerCompatibilityError,
                             ErrorCode::Canceled,
                             ErrorCode::PermissionDenied }) {
        SCOPED_TRACE(static_cast<int>(code));
        EXPECT_TRUE(cli->handleCommonError(makeError(code)));
    }

    EXPECT_FALSE(cli->handleCommonError(makeError(ErrorCode::InvalidFuzzyReference, "bad ref")));
    ASSERT_EQ(printer.messages.size(), 4U);
    EXPECT_THAT(printer.messages[0], testing::HasSubstr("Network connection failed"));
    EXPECT_THAT(printer.messages[1], testing::HasSubstr("Package not found"));
    EXPECT_THAT(printer.messages[2], testing::HasSubstr("Operation canceled"));
    EXPECT_THAT(printer.messages[3], testing::HasSubstr("Permission denied"));
    ASSERT_EQ(printer.errors.size(), 1U);
    EXPECT_EQ(printer.errors.front(), "bad ref");
}

TEST_F(CliPrivateTest, InstallHandlersCoverAllErrorCodes)
{
    v1::PackageManager1InstallParameters params;
    params.package.id = "org.example.app";

    const std::vector<ErrorCode> codes{
        ErrorCode::AppInstallModuleRequireAppFirst,
        ErrorCode::AppInstallModuleAlreadyExists,
        ErrorCode::AppInstallModuleNotFound,
        ErrorCode::AppInstallAlreadyInstalled,
        ErrorCode::AppInstallNotFoundFromRemote,
        ErrorCode::AppInstallModuleNoVersion,
        ErrorCode::AppInstallNeedDowngrade,
        ErrorCode::AppInstallFailed,
        ErrorCode::Unknown,
    };
    for (const auto code : codes) {
        SCOPED_TRACE(static_cast<int>(code));
        cli->handleInstallError(makeError(code), params);
    }

    cli->handleInstallError(makeError(ErrorCode::NetworkError), params);
    cli->handleInstallFromFileError(makeError(ErrorCode::AppInstallAlreadyInstalled));
    cli->handleInstallFromFileError(makeError(ErrorCode::Canceled));

    EXPECT_GE(printer.messages.size(), 11U);
    EXPECT_THAT(printer.messages.front(), testing::HasSubstr("first install the app"));
    EXPECT_THAT(printer.messages[4], testing::HasSubstr("org.example.app is not found"));
    EXPECT_THAT(printer.messages[6], testing::HasSubstr("--force"));
    EXPECT_THAT(printer.messages[7], testing::HasSubstr("Install failed"));
    EXPECT_TRUE(printer.errors.empty());

    cli->setGlobalOptions(cli::GlobalOptions{ .verbose = 1 });
    cli->handleInstallError(makeError(ErrorCode::AppInstallFailed, "verbose failure"), params);
    ASSERT_EQ(printer.errors.size(), 1U);
    EXPECT_EQ(printer.errors.front(), "verbose failure");
}

TEST_F(CliPrivateTest, UninstallAndUpgradeHandlersCoverAllErrorCodes)
{
    for (const auto code : { ErrorCode::AppUninstallAppIsRunning,
                             ErrorCode::AppUninstallNotFoundFromLocal,
                             ErrorCode::AppUninstallMultipleVersions,
                             ErrorCode::AppUninstallBaseOrRuntime,
                             ErrorCode::AppUninstallFailed,
                             ErrorCode::Unknown,
                             ErrorCode::NetworkError }) {
        SCOPED_TRACE(static_cast<int>(code));
        cli->handleUninstallError(makeError(code, "details"));
    }

    for (const auto code : { ErrorCode::AppUpgradeLocalNotFound,
                             ErrorCode::AppUpgradeFailed,
                             ErrorCode::Unknown,
                             ErrorCode::PermissionDenied }) {
        SCOPED_TRACE(static_cast<int>(code));
        cli->handleUpgradeError(makeError(code));
    }

    ASSERT_GE(printer.messages.size(), 9U);
    EXPECT_THAT(printer.messages[0], testing::HasSubstr("not installed"));
    EXPECT_THAT(printer.messages[1], testing::HasSubstr("Multiple versions"));
    EXPECT_THAT(printer.messages[2], testing::HasSubstr("Base or runtime"));
    EXPECT_THAT(printer.messages[3], testing::HasSubstr("Uninstall failed"));
    EXPECT_THAT(printer.messages[7], testing::HasSubstr("Upgrade failed"));
}

TEST_F(CliPrivateTest, SearchResultFiltersDevelopAndSelectsLatestApp)
{
    const v1::PackageManager1SearchResult result{
        .packages =
          std::map<std::string, std::vector<v1::PackageInfoV2>>{
            { "stable",
              { makePackage("org.example.app", "1.0.0"),
                makePackage("org.example.app", "2.0.0"),
                makePackage("org.example.app", "1.1.0", "app", "develop"),
                makePackage("org.example.base", "1.0.0", "base") } } },
        .code = 0,
        .message = "",
        .type = "",
    };

    ASSERT_TRUE(cli->handleSearchResult(common::serialize::toQVariantMap(result),
                                        cli::SearchOptions{ .type = "app" }));
    ASSERT_EQ(printer.searchResults.size(), 1U);
    const auto &packages = printer.searchResults.back();
    ASSERT_EQ(packages.size(), 1U);
    ASSERT_EQ(packages.at("stable").size(), 1U);
    EXPECT_EQ(packages.at("stable").front().version, "2.0.0");

    ASSERT_TRUE(cli->handleSearchResult(
      common::serialize::toQVariantMap(result),
      cli::SearchOptions{ .type = "all", .showDevel = true, .showAllVersion = true }));
    ASSERT_EQ(printer.searchResults.back().at("stable").size(), 4U);
}

TEST_F(CliPrivateTest, SearchAndCommonResultsRejectMalformedPayloads)
{
    EXPECT_FALSE(cli->handleSearchResult(QVariantMap{}, cli::SearchOptions{}));
    EXPECT_FALSE(cli->printCommonResult(QVariantMap{}));
    ASSERT_EQ(printer.errors.size(), 2U);

    const v1::CommonResult result{ .code = 0, .message = "installed", .type = "" };
    EXPECT_TRUE(cli->printCommonResult(common::serialize::toQVariantMap(result)));
    ASSERT_FALSE(printer.messages.empty());
    EXPECT_EQ(printer.messages.back(), "installed");
}

TEST_F(CliPrivateTest, ContainerIDMatchingAcceptsUniqueSuffixes)
{
    EXPECT_TRUE(cli->isContainerIDMatch("1234567890abcdef", "1234567890ab"));
    EXPECT_TRUE(cli->isContainerIDMatch("90abcdef", "90abcdef"));
    EXPECT_FALSE(cli->isContainerIDMatch("1234567890abcdef", "not-present"));
    EXPECT_FALSE(cli->isContainerIDMatch("abcdef123456", "abcdef"));
    EXPECT_FALSE(cli->isContainerIDMatch("1234567890abcdef", "abcdef123456"));
}

} // namespace
