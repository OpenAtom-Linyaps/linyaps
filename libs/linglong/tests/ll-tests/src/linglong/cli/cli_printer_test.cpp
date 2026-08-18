/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/cli/cli_printer.h"
#include "linglong/cli/json_printer.h"

#include <QStringList>

#include <sstream>
#include <string>

using namespace linglong::api::types::v1;

namespace {

class CaptureStdio
{
public:
    CaptureStdio()
        : oldCout_(std::cout.rdbuf(coutStream_.rdbuf()))
        , oldCerr_(std::cerr.rdbuf(cerrStream_.rdbuf()))
    {
    }

    ~CaptureStdio()
    {
        std::cout.rdbuf(oldCout_);
        std::cerr.rdbuf(oldCerr_);
    }

    std::string str() const { return coutStream_.str() + cerrStream_.str(); }

private:
    std::ostringstream coutStream_;
    std::ostringstream cerrStream_;
    std::streambuf *oldCout_;
    std::streambuf *oldCerr_;
};

using CaptureStdout = CaptureStdio;

PackageInfoV2 makePackage(const char *id,
                          const char *name,
                          const char *version,
                          const char *channel,
                          const char *module,
                          const char *description)
{
    PackageInfoV2 pkg;
    pkg.id = id;
    pkg.name = name;
    pkg.version = version;
    pkg.channel = channel;
    pkg.packageInfoV2Module = module;
    pkg.description = description;
    pkg.schemaVersion = "1.0";
    pkg.kind = "app";
    pkg.size = 0;
    return pkg;
}

TEST(CLIPrinter, PrintErr)
{
    auto err = linglong::utils::error::Error::Err("file.cpp", 42, "trace", "boom", -7);
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printErr(err);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("Error -7"));
    EXPECT_THAT(out, ::testing::HasSubstr("boom"));
}

TEST(CLIPrinter, PrintPackage)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "a demo app");
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printPackage(pkg);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("1.0.0"));
}

TEST(CLIPrinter, PrintPackagesWithHeader)
{
    PackageInfoDisplay pkg;
    pkg.id = "org.deepin.demo";
    pkg.name = "Demo Application Name";
    pkg.version = "1.0.0";
    pkg.channel = "stable";
    pkg.packageInfoDisplayModule = "binary";
    pkg.description = "A very interesting demo application used for testing";

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printPackages(std::vector<PackageInfoDisplay>{ pkg });
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("1.0.0"));
}

TEST(CLIPrinter, PrintSearchResultEmpty)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printSearchResult({});
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("No packages"));
}

TEST(CLIPrinter, PrintSearchResultSorts)
{
    auto older = makePackage("org.deepin.a", "A", "1.0.0", "stable", "binary", "older");
    auto newer = makePackage("org.deepin.a", "A", "2.0.0", "stable", "binary", "newer");

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printSearchResult(std::map<std::string, std::vector<PackageInfoV2>>{
      { "repo1", { older, newer } },
    });
    auto out = capture.str();
    // newer version (2.0.0) must be printed before older (1.0.0)
    EXPECT_LT(out.find("2.0.0"), out.find("1.0.0"));
}

TEST(CLIPrinter, PrintPruneResultEmpty)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printPruneResult({});
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("No unused"));
}

TEST(CLIPrinter, PrintContainersEmpty)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printContainers({});
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("No containers"));
}

TEST(CLIPrinter, PrintContainers)
{
    CliContainer c1;
    c1.id = "abcdef123456";
    c1.package = "org.deepin.demo/demo";
    c1.pid = 1234;

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printContainers(std::vector<CliContainer>{ c1 });
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("abcdef123456"));
    EXPECT_THAT(out, ::testing::HasSubstr("1234"));
}

TEST(CLIPrinter, PrintRepoConfig)
{
    RepoConfigV2 cfg;
    cfg.defaultRepo = "repo1";
    Repo repo1;
    repo1.name = "repo1";
    repo1.url = "https://mirrors.example.com/linglong/repo1";
    repo1.priority = 10;
    Repo repo2;
    repo2.name = "repo2";
    repo2.url = "https://mirrors.example.com/linglong/repo2";
    repo2.priority = 5;
    cfg.repos = { repo1, repo2 };

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printRepoConfig(cfg);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("Default: repo1"));
    // sorted by priority desc: repo1 before repo2
    EXPECT_LT(out.find("repo1"), out.find("repo2"));
}

TEST(CLIPrinter, PrintLayerInfo)
{
    LayerInfo info;
    info.info = nlohmann::json{ { "id", "org.deepin.demo" } };

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printLayerInfo(info);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST(CLIPrinter, PrintContent)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printContent(QStringList{ "/usr/bin/demo", "/usr/share/demo" });
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("/usr/bin/demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("/usr/share/demo"));
}

TEST(CLIPrinter, PrintProgress)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printProgress(42.5, "downloading");
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("downloading"));
    EXPECT_THAT(out, ::testing::HasSubstr("42.50%"));
}

TEST(CLIPrinter, PrintUpgradeListEmpty)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    std::vector<UpgradeListResult> list;
    printer.printUpgradeList(list);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("No apps"));
}

TEST(CLIPrinter, PrintUpgradeList)
{
    UpgradeListResult item;
    item.id = "org.deepin.demo";
    item.oldVersion = "1.0.0";
    item.newVersion = "2.0.0";

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    std::vector<UpgradeListResult> list{ item };
    printer.printUpgradeList(list);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("1.0.0"));
    EXPECT_THAT(out, ::testing::HasSubstr("2.0.0"));
}

TEST(CLIPrinter, PrintInspect)
{
    InspectResult result;
    result.appID = "org.deepin.demo";

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printInspect(result);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST(CLIPrinter, PrintInspectNoApp)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printInspect(InspectResult{});
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("none"));
}

TEST(CLIPrinter, PrintModuleSizes)
{
    linglong::cli::Printer::ModuleSizeInfo mod;
    mod.id = "org.deepin.demo";
    mod.version = "1.0.0";
    mod.channel = "stable";
    mod.module = "binary";
    mod.exclusiveSize = 1024;
    mod.sharedSize = 2048;
    mod.logicalSize = 4096;
    mod.actualSize = 8192;

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printModuleSizes(std::vector<linglong::cli::Printer::ModuleSizeInfo>{ mod },
                             8192,
                             16384);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("1.0 KiB"));
}

TEST(CLIPrinter, PrintDepends)
{
    linglong::cli::Printer::DependsNode child;
    child.ref = "org.deepin.child/base";
    child.kind = "runtime";

    linglong::cli::Printer::DependsNode root;
    root.ref = "org.deepin.demo/demo";
    root.children = { child };

    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printDepends(std::vector<linglong::cli::Printer::DependsNode>{ root });
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.child"));
    EXPECT_THAT(out, ::testing::HasSubstr("runtime"));
}

TEST(CLIPrinter, PrintMessageAndClearLine)
{
    CaptureStdout capture;
    linglong::cli::CLIPrinter printer;
    printer.printMessage("hello world");
    printer.clearLine();
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("hello world"));
}

// ---------------------------------------------------------------- JSONPrinter

class JSONPrinterTest : public ::testing::Test
{
protected:
    linglong::cli::JSONPrinter printer;
};

TEST_F(JSONPrinterTest, PrintErr)
{
    auto err = linglong::utils::error::Error::Err("file.cpp", 42, "trace", "boom", -7);
    CaptureStdout capture;
    printer.printErr(err);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("\"message\""));
    EXPECT_THAT(out, ::testing::HasSubstr("boom"));
}

TEST_F(JSONPrinterTest, PrintPackage)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "a demo app");
    CaptureStdout capture;
    printer.printPackage(pkg);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("1.0.0"));
}

TEST_F(JSONPrinterTest, PrintPackagesDisplay)
{
    PackageInfoDisplay pkg;
    pkg.id = "org.deepin.demo";
    pkg.name = "Demo";
    pkg.version = "1.0.0";
    pkg.channel = "stable";

    CaptureStdout capture;
    printer.printPackages(std::vector<PackageInfoDisplay>{ pkg });
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintSearchResult)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "a demo app");
    CaptureStdout capture;
    printer.printSearchResult(
      std::map<std::string, std::vector<PackageInfoV2>>{ { "repo1", { pkg } } });
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintPruneResult)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "");
    CaptureStdout capture;
    printer.printPruneResult(std::vector<PackageInfoV2>{ pkg });
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintContainers)
{
    CliContainer c1;
    c1.id = "abcdef123456";
    c1.package = "org.deepin.demo/demo";
    c1.pid = 1234;
    CaptureStdout capture;
    printer.printContainers(std::vector<CliContainer>{ c1 });
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("abcdef123456"));
}

TEST_F(JSONPrinterTest, PrintRepoConfig)
{
    RepoConfigV2 cfg;
    cfg.defaultRepo = "repo1";
    Repo repo1;
    repo1.name = "repo1";
    repo1.url = "https://mirrors.example.com/linglong/repo1";
    repo1.priority = 10;
    cfg.repos = { repo1 };
    CaptureStdout capture;
    printer.printRepoConfig(cfg);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("repo1"));
}

TEST_F(JSONPrinterTest, PrintLayerInfo)
{
    LayerInfo info;
    info.info = nlohmann::json{ { "id", "org.deepin.demo" } };
    CaptureStdout capture;
    printer.printLayerInfo(info);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintContent)
{
    CaptureStdout capture;
    printer.printContent(QStringList{ "/usr/bin/demo" });
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("/usr/bin/demo"));
}

TEST_F(JSONPrinterTest, PrintProgress)
{
    CaptureStdout capture;
    printer.printProgress(12.5, "pulling");
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("pulling"));
    EXPECT_THAT(out, ::testing::HasSubstr("12.5"));
}

TEST_F(JSONPrinterTest, PrintUpgradeList)
{
    UpgradeListResult item;
    item.id = "org.deepin.demo";
    item.oldVersion = "1.0.0";
    item.newVersion = "2.0.0";
    CaptureStdout capture;
    std::vector<UpgradeListResult> list{ item };
    printer.printUpgradeList(list);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintInspect)
{
    InspectResult result;
    result.appID = "org.deepin.demo";
    CaptureStdout capture;
    printer.printInspect(result);
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("org.deepin.demo"));
}

TEST_F(JSONPrinterTest, PrintModuleSizes)
{
    linglong::cli::Printer::ModuleSizeInfo mod;
    mod.id = "org.deepin.demo";
    mod.version = "1.0.0";
    mod.channel = "stable";
    mod.module = "binary";
    mod.exclusiveSize = 1024;
    mod.sharedSize = 2048;
    mod.logicalSize = 4096;
    mod.actualSize = 8192;
    CaptureStdout capture;
    printer.printModuleSizes(std::vector<linglong::cli::Printer::ModuleSizeInfo>{ mod },
                             8192,
                             16384);
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("calculatedLogicalSize"));
}

TEST_F(JSONPrinterTest, PrintDepends)
{
    linglong::cli::Printer::DependsNode child;
    child.ref = "org.deepin.child/base";

    linglong::cli::Printer::DependsNode root;
    root.ref = "org.deepin.demo/demo";
    root.children = { child };

    CaptureStdout capture;
    printer.printDepends(std::vector<linglong::cli::Printer::DependsNode>{ root });
    auto out = capture.str();
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.demo"));
    EXPECT_THAT(out, ::testing::HasSubstr("org.deepin.child"));
}

TEST_F(JSONPrinterTest, PrintMessage)
{
    CaptureStdout capture;
    printer.printMessage("hi");
    EXPECT_THAT(capture.str(), ::testing::HasSubstr("hi"));
}

} // namespace
