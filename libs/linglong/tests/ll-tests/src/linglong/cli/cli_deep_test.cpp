/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/types/v1/InteractionReply.hpp"
#include "linglong/api/types/v1/InteractionRequest.hpp"
#include "linglong/cli/cli_printer.h"
#include "linglong/cli/dbus_notifier.h"
#include "linglong/cli/dummy_notifier.h"
#include "linglong/cli/interactive_notifier.h"
#include "linglong/cli/json_printer.h"
#include "linglong/cli/terminal_notifier.h"

#include <sstream>
#include <string>
#include <vector>

using namespace linglong::api::types::v1;
using namespace testing;

namespace linglong::cli::test {

namespace {

class CaptureOutputBuffer
{
public:
    CaptureOutputBuffer()
        : oldCout_(std::cout.rdbuf(coutStream_.rdbuf()))
        , oldCerr_(std::cerr.rdbuf(cerrStream_.rdbuf()))
    {
    }

    ~CaptureOutputBuffer()
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

TEST(CliNotifierDeepSuite, DummyNotifierAlwaysSucceeds)
{
    DummyNotifier notifier;

    InteractionRequest req;
    req.interactionRequestPackage = "org.deepin.test";
    req.subject = "Permission request";
    req.body = "Allow access to network?";
    req.options = { "Yes", "No" };

    auto notifyRes = notifier.notify(req);
    EXPECT_TRUE(notifyRes.has_value());

    auto reqRes = notifier.request(req);
    EXPECT_TRUE(reqRes.has_value());
    EXPECT_TRUE(reqRes->canceled);
}

TEST(CliNotifierDeepSuite, InteractionRequestModelFields)
{
    InteractionRequest req;
    req.interactionRequestPackage = "org.deepin.demo";
    req.subject = "Installation Confirmation";
    req.body = "Do you want to install additional runtime dependencies?";
    req.options = { "Continue", "Cancel" };

    EXPECT_EQ(req.interactionRequestPackage, "org.deepin.demo");
    EXPECT_EQ(req.subject, "Installation Confirmation");
    EXPECT_EQ(req.options.size(), 2);
    EXPECT_EQ(req.options[0], "Continue");
    EXPECT_EQ(req.options[1], "Cancel");

    InteractionReply reply;
    reply.canceled = false;
    reply.selected = 0;
    EXPECT_FALSE(reply.canceled);
    EXPECT_EQ(reply.selected, 0);

    InteractionReply cancelReply;
    cancelReply.canceled = true;
    cancelReply.selected = -1;
    EXPECT_TRUE(cancelReply.canceled);
}

TEST(CliNotifierDeepSuite, CLIPrinterEmptyPackageDisplays)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    std::vector<PackageInfoDisplay> emptyList;
    printer.printPackages(emptyList);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("ID"));
    EXPECT_THAT(out, HasSubstr("NAME"));
}

TEST(CliNotifierDeepSuite, CLIPrinterMultipleContainersTableFormatting)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    CliContainer c1{ .id = "cont-111", .package = "app.one/1.0", .pid = 1001 };
    CliContainer c2{ .id = "cont-222", .package = "app.two/2.0", .pid = 2002 };
    CliContainer c3{ .id = "cont-333", .package = "app.three/3.0", .pid = 3003 };

    std::vector<CliContainer> list{ c1, c2, c3 };
    printer.printContainers(list);

    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("cont-111"));
    EXPECT_THAT(out, HasSubstr("cont-222"));
    EXPECT_THAT(out, HasSubstr("cont-333"));
    EXPECT_THAT(out, HasSubstr("1001"));
    EXPECT_THAT(out, HasSubstr("2002"));
    EXPECT_THAT(out, HasSubstr("3003"));
}

TEST(CliNotifierDeepSuite, CLIPrinterSearchMultipleRepositories)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    PackageInfoV2 pkg1{
        .arch = { "x86_64" },
        .channel = "main",
        .id = "org.deepin.terminal",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "6.0.0",
    };
    PackageInfoV2 pkg2{
        .arch = { "x86_64" },
        .channel = "main",
        .id = "org.deepin.terminal",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "5.5.0",
    };
    PackageInfoV2 pkgBeta{
        .arch = { "x86_64" },
        .channel = "beta",
        .id = "org.deepin.terminal",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "6.1.0-alpha",
    };

    std::map<std::string, std::vector<PackageInfoV2>> searchResults{
        { "official-stable", { pkg1, pkg2 } },
        { "official-beta", { pkgBeta } },
    };

    printer.printSearchResult(searchResults);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("official-stable"));
    EXPECT_THAT(out, HasSubstr("official-beta"));
    EXPECT_THAT(out, HasSubstr("org.deepin.terminal"));
}

TEST(CliNotifierDeepSuite, JSONPrinterEmptyCollectionsStructure)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    printer.printContainers({});
    printer.printSearchResult({});
    printer.printPruneResult({});

    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("[]"));
}

TEST(CliNotifierDeepSuite, JSONPrinterComplexUpgradeList)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    UpgradeListResult u1{ .id = "org.app.editor", .newVersion = "2.0.0", .oldVersion = "1.0.0" };
    UpgradeListResult u2{ .id = "org.app.viewer", .newVersion = "3.2.1", .oldVersion = "3.2.0" };

    std::vector<UpgradeListResult> list{ u1, u2 };
    printer.printUpgradeList(list);

    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("\"id\""));
    EXPECT_THAT(out, HasSubstr("org.app.editor"));
    EXPECT_THAT(out, HasSubstr("org.app.viewer"));
    EXPECT_THAT(out, HasSubstr("2.0.0"));
    EXPECT_THAT(out, HasSubstr("3.2.1"));
}

TEST(CliNotifierDeepSuite, JSONPrinterProgressBoundaryPercentages)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    printer.printProgress(0.0, "Starting pull");
    printer.printProgress(100.0, "Completed");

    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("Starting pull"));
    EXPECT_THAT(out, HasSubstr("0"));
    EXPECT_THAT(out, HasSubstr("Completed"));
    EXPECT_THAT(out, HasSubstr("100"));
}

TEST(CliNotifierDeepSuite, CLIPrinterLayerInfoJsonOutput)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    LayerInfo layer;
    layer.info = nlohmann::json{
        { "arch", { "x86_64", "aarch64" } },
        { "channel", "stable" },
        { "id", "org.deepin.base" },
        { "version", "23.0.0" },
    };

    printer.printLayerInfo(layer);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.base"));
    EXPECT_THAT(out, HasSubstr("23.0.0"));
    EXPECT_THAT(out, HasSubstr("stable"));
}

TEST(CliNotifierDeepSuite, CLIPrinterDependencyForestOutput)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    Printer::DependsNode leaf1{ .children = {}, .kind = "runtime", .ref = "lib.runtime.core/1.0" };
    Printer::DependsNode leaf2{ .children = {},
                                .kind = "extension",
                                .ref = "ext.plugin.media/1.0" };
    Printer::DependsNode intermediate{
        .children = { leaf1, leaf2 },
        .kind = "base",
        .ref = "mid.component.service/2.0",
    };
    Printer::DependsNode root{
        .children = { intermediate },
        .kind = "app",
        .ref = "main.desktop.suite/3.0",
    };

    printer.printDepends({ root });
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("main.desktop.suite/3.0"));
    EXPECT_THAT(out, HasSubstr("mid.component.service/2.0"));
    EXPECT_THAT(out, HasSubstr("lib.runtime.core/1.0"));
    EXPECT_THAT(out, HasSubstr("ext.plugin.media/1.0"));
}

TEST(CliNotifierDeepSuite, CLIPrinterInspectCompleteFields)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    InspectResult res;
    res.appID = "org.deepin.calculator";
    res.arch = "x86_64";
    res.channel = "main";
    res.version = "1.2.3";
    res.module = "binary";
    res.kind = "app";

    printer.printInspect(res);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.calculator"));
    EXPECT_THAT(out, HasSubstr("x86_64"));
    EXPECT_THAT(out, HasSubstr("1.2.3"));
    EXPECT_THAT(out, HasSubstr("binary"));
}

TEST(CliNotifierDeepSuite, CLIPrinterInspectEmptyResult)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    InspectResult emptyRes;
    printer.printInspect(emptyRes);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("none"));
}

TEST(CliNotifierDeepSuite, CLIPrinterModuleSizesMultipleEntries)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    Printer::ModuleSizeInfo m1{
        .actualSize = 1048576,
        .channel = "stable",
        .exclusiveSize = 524288,
        .id = "app.deepin.music",
        .logicalSize = 2097152,
        .module = "binary",
        .sharedSize = 524288,
        .version = "1.0.0",
    };
    Printer::ModuleSizeInfo m2{
        .actualSize = 2097152,
        .channel = "stable",
        .exclusiveSize = 1048576,
        .id = "app.deepin.music",
        .logicalSize = 4194304,
        .module = "develop",
        .sharedSize = 1048576,
        .version = "1.0.0",
    };

    printer.printModuleSizes({ m1, m2 }, 3145728, 6291456);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("app.deepin.music"));
    EXPECT_THAT(out, HasSubstr("binary"));
    EXPECT_THAT(out, HasSubstr("develop"));
}

TEST(CliNotifierDeepSuite, JSONPrinterModuleSizesSerialization)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    Printer::ModuleSizeInfo m{
        .actualSize = 5000,
        .channel = "main",
        .exclusiveSize = 2500,
        .id = "org.deepin.reader",
        .logicalSize = 10000,
        .module = "binary",
        .sharedSize = 2500,
        .version = "2.1.0",
    };

    printer.printModuleSizes({ m }, 5000, 10000);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.reader"));
    EXPECT_THAT(out, HasSubstr("\"totalSize\""));
    EXPECT_THAT(out, HasSubstr("\"repoSize\""));
}

TEST(CliNotifierDeepSuite, JSONPrinterInspectCompleteStructure)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    InspectResult res;
    res.appID = "org.deepin.draw";
    res.arch = "x86_64";
    res.channel = "main";
    res.version = "5.0.0";
    res.module = "binary";
    res.kind = "app";

    printer.printInspect(res);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.draw"));
    EXPECT_THAT(out, HasSubstr("5.0.0"));
    EXPECT_THAT(out, HasSubstr("main"));
}

TEST(CliNotifierDeepSuite, JSONPrinterInspectEmptyStructure)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    InspectResult emptyRes;
    printer.printInspect(emptyRes);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("{}"));
}

TEST(CliNotifierDeepSuite, CLIPrinterRepoConfigEmptyRepos)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    RepoConfigV2 cfg;
    cfg.defaultRepo = "default_only";
    cfg.repos = {};

    printer.printRepoConfig(cfg);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("Default: default_only"));
}

TEST(CliNotifierDeepSuite, JSONPrinterRepoConfigEmptyRepos)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    RepoConfigV2 cfg;
    cfg.defaultRepo = "default_only";
    cfg.repos = {};

    printer.printRepoConfig(cfg);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("default_only"));
    EXPECT_THAT(out, HasSubstr("\"repos\":[]"));
}

TEST(CliNotifierDeepSuite, CLIPrinterSinglePackageDetails)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    PackageInfoV2 pkg;
    pkg.id = "org.deepin.screenshot";
    pkg.name = "Deepin Screenshot";
    pkg.version = "5.0.1";
    pkg.channel = "main";
    pkg.packageInfoV2Module = "binary";
    pkg.description = "Full featured desktop screen capture utility";
    pkg.kind = "app";

    printer.printPackage(pkg);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.screenshot"));
    EXPECT_THAT(out, HasSubstr("5.0.1"));
    EXPECT_THAT(out, HasSubstr("Deepin Screenshot"));
}

TEST(CliNotifierDeepSuite, JSONPrinterSinglePackageDetails)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    PackageInfoV2 pkg;
    pkg.id = "org.deepin.screenshot";
    pkg.name = "Deepin Screenshot";
    pkg.version = "5.0.1";
    pkg.channel = "main";
    pkg.packageInfoV2Module = "binary";
    pkg.description = "Full featured desktop screen capture utility";
    pkg.kind = "app";

    printer.printPackage(pkg);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.screenshot"));
    EXPECT_THAT(out, HasSubstr("5.0.1"));
    EXPECT_THAT(out, HasSubstr("\"id\""));
}

TEST(CliNotifierDeepSuite, CLIPrinterMultipleDesktopPaths)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    QStringList paths{
        "/usr/share/applications/org.deepin.browser.desktop",
        "/usr/share/applications/org.deepin.editor.desktop",
        "/usr/share/applications/org.deepin.terminal.desktop",
    };

    printer.printContent(paths);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.browser.desktop"));
    EXPECT_THAT(out, HasSubstr("org.deepin.editor.desktop"));
    EXPECT_THAT(out, HasSubstr("org.deepin.terminal.desktop"));
}

TEST(CliNotifierDeepSuite, JSONPrinterMultipleDesktopPaths)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    QStringList paths{
        "/usr/share/applications/org.deepin.browser.desktop",
        "/usr/share/applications/org.deepin.editor.desktop",
    };

    printer.printContent(paths);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("org.deepin.browser.desktop"));
    EXPECT_THAT(out, HasSubstr("org.deepin.editor.desktop"));
}

TEST(CliNotifierDeepSuite, CLIPrinterErrorMessageFormatting)
{
    CaptureOutputBuffer capture;
    CLIPrinter printer;

    auto err =
      linglong::utils::error::Error::Err("test_path.cpp", 123, "stack", "fatal error test", 404);
    printer.printErr(err);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("404"));
    EXPECT_THAT(out, HasSubstr("fatal error test"));
}

TEST(CliNotifierDeepSuite, JSONPrinterErrorMessageFormatting)
{
    CaptureOutputBuffer capture;
    JSONPrinter printer;

    auto err =
      linglong::utils::error::Error::Err("test_path.cpp", 123, "stack", "json error test", 500);
    printer.printErr(err);
    auto out = capture.str();
    EXPECT_THAT(out, HasSubstr("500"));
    EXPECT_THAT(out, HasSubstr("json error test"));
    EXPECT_THAT(out, HasSubstr("\"code\""));
}

} // namespace

} // namespace linglong::cli::test
