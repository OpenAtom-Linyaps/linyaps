/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/package/package.h"

//
#include "linglong/cli/dbus_reply.h"
//

#include "linglong/api/dbus/v1/mock_app_manager.h"
#include "linglong/api/dbus/v1/mock_package_manager.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/mock_printer.h"
#include "linglong/dbus_ipc/reply.h"
#include "linglong/package_manager/mock_package_manager.h"
#include "linglong/utils/finally/finally.h"

#include <docopt.h>

#include <iostream>

#include <wordexp.h>

namespace linglong::cli::test {

namespace {

std::map<std::string, docopt::value> parseCommand(const QString &command)
{
    int argc = 0;
    char **argv = nullptr;
    auto words = command.toStdString();
    wordexp_t exp{};
    wordexp(words.c_str(), &exp, 0);
    auto defer = utils::finally::finally([&exp]() {
        wordfree(&exp);
    });

    argc = (int)exp.we_wordc;
    argv = exp.we_wordv;
    auto args = docopt::docopt(Cli::USAGE,
                               { argv + 1, argv + argc },
                               false,                 // show help if requested
                               "linglong CLI 1.4.0"); // version string
    return args;
}

} // namespace

using ::testing::StrictMock;

class CLITest : public ::testing::Test
{
protected:
    std::unique_ptr<StrictMock<api::dbus::v1::test::MockAppManager>> appMan;
    std::unique_ptr<StrictMock<api::dbus::v1::test::MockPackageManager>> pkgMan;
    std::unique_ptr<StrictMock<MockPrinter>> printer;
    std::unique_ptr<StrictMock<service::test::MockPackageManager>> pkgManImpl;
    std::unique_ptr<Cli> cli;

    static void SetUpTestSuite() { registerDBusParam(); }

    void SetUp() override
    {
        printer = std::make_unique<StrictMock<MockPrinter>>();
        appMan = std::make_unique<StrictMock<api::dbus::v1::test::MockAppManager>>(
          "org.deepin.linglong.AppManager",
          "/org/deepin/linglong/AppManager",
          QDBusConnection::sessionBus());
        pkgMan = std::make_unique<StrictMock<api::dbus::v1::test::MockPackageManager>>(
          "org.deepin.linglong.AppManager",
          "/org/deepin/linglong/AppManager",
          QDBusConnection::sessionBus());
        pkgManImpl = std::make_unique<StrictMock<service::test::MockPackageManager>>();

        cli = std::make_unique<Cli>(*printer, *appMan, *pkgMan, *pkgManImpl);
    }

    void TearDown() override
    {
        appMan = nullptr;
        pkgMan = nullptr;
        printer = nullptr;
    }
};

using ::testing::Return;

TEST_F(CLITest, Run)
{
    auto args = parseCommand("ll-cli run com.163.music");
    EXPECT_CALL(*appMan, Start).Times(1).WillOnce(Return(createReply(service::Reply{ 0, "" })));

    auto ret = cli->run(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Exec)
{
    auto args = parseCommand("ll-cli exec com.163.music ls");
    EXPECT_CALL(*appMan, Exec).Times(1).WillOnce(Return(createReply(service::Reply{ 0, "" })));

    auto ret = cli->exec(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Enter)
{
    GTEST_SKIP() << "Skip test enter as that command is not implemented yet.";

    auto args = parseCommand("ll-cli enter xxxx");

    auto ret = cli->enter(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Ps)
{
    auto args = parseCommand("ll-cli ps");
    EXPECT_CALL(*appMan, ListContainer)
      .Times(1)
      .WillOnce(Return(createReply(service::QueryReply{ { 0, "" }, {} })));
    EXPECT_CALL(*printer, print(QList<QSharedPointer<Container>>{}));

    auto ret = cli->ps(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Kill)
{
    auto args = parseCommand("ll-cli kill xxxx");

    EXPECT_CALL(*appMan, Stop)
      .Times(1)
      .WillOnce(Return(createReply(service::Reply{ STATUS_CODE(kErrorPkgKillSuccess), "" })));

    auto ret = cli->kill(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Install)
{
    auto args = parseCommand("ll-cli install \'xxxx\'");
    EXPECT_CALL(*pkgMan, Install).Times(1).WillOnce(Return(createReply(service::Reply{ 0, "" })));
    EXPECT_CALL(*pkgMan, GetDownloadStatus)
      .Times(1)
      .WillOnce(Return(createReply(service::Reply{ STATUS_CODE(kPkgInstallSuccess), "" })));

    auto ret = cli->install(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Upgrade)
{
    auto args = parseCommand("ll-cli upgrade \'xxxx\'");
    EXPECT_CALL(*pkgMan, Update)
      .Times(1)
      .WillOnce(Return(createReply(service::Reply{ STATUS_CODE(kErrorPkgUpdateSuccess), "" })));

    auto ret = cli->upgrade(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Search)
{
    auto args = parseCommand("ll-cli search xxxx");
    EXPECT_CALL(*pkgMan, Query)
      .Times(1)
      .WillOnce(Return(
        createReply(service::QueryReply{ { STATUS_CODE(kErrorPkgQuerySuccess), "" }, "[]" })));
    EXPECT_CALL(*printer, print(QList<QSharedPointer<package::AppMetaInfo>>{})).Times(1);

    auto ret = cli->search(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Uninstall)
{
    auto args = parseCommand("ll-cli uninstall \'xxx aaa\'");
    EXPECT_CALL(*pkgMan, Uninstall)
      .Times(1)
      .WillOnce(Return(createReply(service::Reply{ STATUS_CODE(kPkgUninstallSuccess), "" })));

    auto ret = cli->uninstall(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, List)
{
    auto args = parseCommand("ll-cli list");
    EXPECT_CALL(*pkgMan, Query)
      .Times(1)
      .WillOnce(Return(
        createReply(service::QueryReply{ { STATUS_CODE(kErrorPkgQuerySuccess), "" }, "[]" })));
    EXPECT_CALL(*printer, print(QList<QSharedPointer<package::AppMetaInfo>>{})).Times(1);

    auto ret = cli->list(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, repo)
{
    auto args = parseCommand("ll-cli repo xxx");
    EXPECT_CALL(*pkgMan, getRepoInfo)
      .Times(1)
      .WillOnce(
        Return(createReply(service::QueryReply{ { STATUS_CODE(kErrorPkgQuerySuccess), "" }, "" })));

    auto ret = cli->repo(args);

    EXPECT_EQ(ret, 0);
}

} // namespace linglong::cli::test
