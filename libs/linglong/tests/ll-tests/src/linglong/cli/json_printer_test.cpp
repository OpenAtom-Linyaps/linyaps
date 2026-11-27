/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/cli/json_printer.h"
#include "linglong/utils/error/error.h"

#include <nlohmann/json.hpp>

#include <QStringList>

#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace linglong::api::types::v1;

namespace {

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

class JSONPrinterStreamTest : public ::testing::Test
{
protected:
    std::ostringstream stream;
    linglong::cli::JSONPrinter printer{ stream };
};

TEST_F(JSONPrinterStreamTest, PrintErr)
{
    auto err = linglong::utils::error::Error::Err("file.cpp", 42, "trace", "boom", -7);
    printer.printErr(err);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed["code"], -7);
    EXPECT_EQ(parsed["message"], "boom");
}

TEST_F(JSONPrinterStreamTest, PrintPackage)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "a demo app");
    printer.printPackage(pkg);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(pkg));
}

TEST_F(JSONPrinterStreamTest, PrintPackagesDisplay)
{
    PackageInfoDisplay pkg;
    pkg.id = "org.deepin.demo";
    pkg.name = "Demo";
    pkg.version = "1.0.0";
    pkg.channel = "stable";

    printer.printPackages(std::vector<PackageInfoDisplay>{ pkg });

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(std::vector<PackageInfoDisplay>{ pkg }));
}

TEST_F(JSONPrinterStreamTest, PrintPruneResultUsesPackageV2)
{
    auto pkg = makePackage("org.deepin.demo", "Demo", "1.0.0", "stable", "binary", "");
    printer.printPruneResult(std::vector<PackageInfoV2>{ pkg });

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(std::vector<PackageInfoV2>{ pkg }));
}

TEST_F(JSONPrinterStreamTest, PrintSearchResultSortsByVersionDesc)
{
    auto older = makePackage("org.deepin.a", "A", "1.0.0", "stable", "binary", "older");
    auto newer = makePackage("org.deepin.a", "A", "2.0.0", "stable", "binary", "newer");

    printer.printSearchResult(
      std::map<std::string, std::vector<PackageInfoV2>>{ { "repo1", { older, newer } } });

    auto parsed = nlohmann::json::parse(stream.str());
    ASSERT_TRUE(parsed.contains("repo1"));
    ASSERT_EQ(parsed["repo1"].size(), 2);
    EXPECT_EQ(parsed["repo1"][0]["version"], "2.0.0");
    EXPECT_EQ(parsed["repo1"][1]["version"], "1.0.0");
}

TEST_F(JSONPrinterStreamTest, PrintContainers)
{
    CliContainer c1;
    c1.id = "abcdef123456";
    c1.package = "org.deepin.demo/demo";
    c1.pid = 1234;

    printer.printContainers(std::vector<CliContainer>{ c1 });

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(std::vector<CliContainer>{ c1 }));
}

TEST_F(JSONPrinterStreamTest, PrintRepoConfig)
{
    RepoConfigV2 cfg;
    cfg.defaultRepo = "repo1";
    Repo repo1;
    repo1.name = "repo1";
    repo1.url = "https://mirrors.example.com/linglong/repo1";
    repo1.priority = 10;
    cfg.repos = { repo1 };

    printer.printRepoConfig(cfg);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(cfg));
}

TEST_F(JSONPrinterStreamTest, PrintLayerInfo)
{
    LayerInfo info;
    info.info = nlohmann::json{ { "id", "org.deepin.demo" } };

    printer.printLayerInfo(info);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(info));
}

TEST_F(JSONPrinterStreamTest, PrintContent)
{
    printer.printContent(QStringList{ "/usr/bin/demo", "/usr/share/demo" });

    auto parsed = nlohmann::json::parse(stream.str());
    ASSERT_TRUE(parsed.contains("content"));
    EXPECT_EQ(parsed["content"][0], "/usr/bin/demo");
    EXPECT_EQ(parsed["content"][1], "/usr/share/demo");
}

TEST_F(JSONPrinterStreamTest, PrintProgress)
{
    printer.printProgress(12.5, "pulling");

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_DOUBLE_EQ(parsed["percentage"], 12.5);
    EXPECT_EQ(parsed["message"], "pulling");
}

TEST_F(JSONPrinterStreamTest, PrintUpgradeList)
{
    UpgradeListResult item;
    item.id = "org.deepin.demo";
    item.oldVersion = "1.0.0";
    item.newVersion = "2.0.0";
    std::vector<UpgradeListResult> list{ item };

    printer.printUpgradeList(list);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(list));
}

TEST_F(JSONPrinterStreamTest, PrintInspect)
{
    InspectResult result;
    result.appID = "org.deepin.demo";

    printer.printInspect(result);

    auto parsed = nlohmann::json::parse(stream.str());
    EXPECT_EQ(parsed, nlohmann::json(result));
}

TEST_F(JSONPrinterStreamTest, PrintModuleSizes)
{
    linglong::cli::Printer::ModuleSizeInfo info;
    info.id = "org.deepin.demo";
    info.name = "demo";
    info.version = "1.0.0";
    info.channel = "stable";
    info.module = "binary";
    info.exclusiveSize = 1024;
    info.sharedSize = 2048;
    info.logicalSize = 4096;
    info.actualSize = 8192;

    printer.printModuleSizes(std::vector<linglong::cli::Printer::ModuleSizeInfo>{ info },
                             8192,
                             16384);

    auto parsed = nlohmann::json::parse(stream.str());
    ASSERT_TRUE(parsed.contains("modules"));
    ASSERT_EQ(parsed["modules"].size(), 1);
    EXPECT_EQ(parsed["modules"][0]["id"], "org.deepin.demo");
    EXPECT_EQ(parsed["modules"][0]["version"], "1.0.0");
    EXPECT_EQ(parsed["modules"][0]["exclusiveSize"], 1024);
    EXPECT_EQ(parsed["modules"][0]["sharedSize"], 2048);
    EXPECT_EQ(parsed["modules"][0]["logicalSize"], 4096);
    EXPECT_EQ(parsed["modules"][0]["actualSize"], 8192);
    EXPECT_EQ(parsed["calculatedLogicalSize"]["exclusiveSize"], 1024);
    EXPECT_EQ(parsed["calculatedLogicalSize"]["sharedSize"], 2048);
    EXPECT_EQ(parsed["calculatedLogicalSize"]["logicalSize"], 4096);
    EXPECT_EQ(parsed["calculatedActualSize"], 8192);
    EXPECT_EQ(parsed["repositoryRealSize"], 16384);
}

TEST_F(JSONPrinterStreamTest, PrintDepends)
{
    linglong::cli::Printer::DependsNode child;
    child.ref = "org.deepin.child/base";
    child.kind = "runtime";

    linglong::cli::Printer::DependsNode root;
    root.ref = "org.deepin.demo/demo";
    root.kind = "app";
    root.children = { child };

    printer.printDepends(std::vector<linglong::cli::Printer::DependsNode>{ root });

    auto parsed = nlohmann::json::parse(stream.str());
    ASSERT_EQ(parsed.size(), 1);
    EXPECT_EQ(parsed[0]["ref"], "org.deepin.demo/demo");
    EXPECT_EQ(parsed[0]["kind"], "app");
    ASSERT_EQ(parsed[0]["children"].size(), 1);
    EXPECT_EQ(parsed[0]["children"][0]["ref"], "org.deepin.child/base");
    EXPECT_EQ(parsed[0]["children"][0]["kind"], "runtime");
}

} // namespace
