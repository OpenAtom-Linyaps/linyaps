/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "docopt.h"
#include "fake_app_manager.h"
#include "fake_command_helper.h"
#include "fake_package_manager.h"
#include "fake_printer.h"
#include "fake_system_package.h"
#include "linglong/cli/cli.h"
#include "linglong/package_manager/package_manager.h"

#include <iostream>

#include <wordexp.h>

using namespace linglong::cli;

Cli initCli(const QString &command, std::shared_ptr<MockPrinter> printer)
{
    int argc;
    char **argv;
    auto words = command.toStdString();
    wordexp_t p;
    wordexp(words.c_str(), &p, 0);
    argc = (int)p.we_wordc;
    argv = p.we_wordv;

    registerDBusParam();

    DocOptMap args = docopt::docopt(USAGE,
                                    { argv + 1, argv + argc },
                                    false,                 // show help if requested
                                    "linglong CLI 1.4.0"); // version string
    wordfree(&p);

    auto appfn = std::make_shared<Factory<linglong::api::v1::dbus::AppManager1>>([]() {
        return std::make_shared<MockAppManager>("org.deepin.linglong.AppManager",
                                                "/org/deepin/linglong/AppManager",
                                                QDBusConnection::sessionBus());
    });

    auto pkgfn = std::make_shared<Factory<linglong::api::v1::dbus::PackageManager1>>([]() {
        return std::make_shared<MockSystemPackage>("org.deepin.linglong.PackageManager",
                                                   "/org/deepin/linglong/PackageManager",
                                                   QDBusConnection::systemBus());
    });

    auto cmdhelperfn = std::make_shared<Factory<linglong::util::CommandHelper>>([]() {
        return std::make_shared<MockCommandHelper>();
    });

    auto pkgmngfn = std::make_shared<Factory<linglong::service::PackageManager>>([]() {
        return std::make_shared<MockPackageManager>();
    });

    Cli cli(args, printer, appfn, pkgfn, cmdhelperfn, pkgmngfn);
    return cli;
}

TEST(Cli, SubCommandRun)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli run com.163.music", printer);
    auto ret = cli.run();
    EXPECT_EQ(printer->errorList.isEmpty(), 1);
}

TEST(Cli, SubCommandExec)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli exec com.163.music ls", printer);
    cli.exec();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandEnter)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli enter xxxx", printer);
    cli.enter();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandPs)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli ps", printer);
    cli.ps();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandKill)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli kill xxxx", printer);
    cli.kill();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandInstall)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli install \'xxxx\'", printer);
    cli.install();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandUpgrade)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli upgrade \'xxxx\'", printer);
    cli.upgrade();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandSearch)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli search xxxx", printer);
    cli.search();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandUninstall)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli uninstall \'xxx aaa\'", printer);
    cli.uninstall();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandList)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli list", printer);
    cli.list();
    EXPECT_EQ(printer->errorList.size(), 0);
}

TEST(Cli, SubCommandRepo)
{
    auto printer = std::make_shared<MockPrinter>();
    auto cli = initCli("ll-cli repo xxxx", printer);
    cli.repo();
    EXPECT_EQ(printer->errorList.size(), 0);
}