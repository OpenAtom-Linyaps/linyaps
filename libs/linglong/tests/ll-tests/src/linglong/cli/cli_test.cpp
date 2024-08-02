/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/api/dbus/v1/mock_package_manager.h"
#include "linglong/cli/cli.h"
#include "linglong/cli/dbus_reply.h"
#include "linglong/cli/mock_printer.h"
#include "linglong/dbus_ipc/reply.h"
#include "linglong/package_manager/mock_package_manager.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/finally/finally.h"

#include <docopt.h>

#include <iostream>

#include <wordexp.h>

namespace linglong::cli::test {

TEST(SubwstrTest, WithinWidthLimit) {
    std::wstring input = L"short";
    int width = 10;
    std::wstring result = subwstr(input, width);
    EXPECT_EQ(result, input);
}

TEST(SubwstrTest, TrimmedString) {
    std::wstring input = L"this is a very long string";
    int width = 10;
    std::wstring expected = L"this is a ";
    std::wstring result = subwstr(input, width);
    EXPECT_EQ(result, expected);
}

TEST(SubwstrTest, WideCharacters) {
    std::wstring input = L"这是一个非常长的字符串";
    int width = 10;
    std::wstring expected = L"这是一个非";
    std::wstring result = subwstr(input, width);
    EXPECT_EQ(result, expected);
}

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
    std::unique_ptr<StrictMock<MockAppManager>> appMan;
    std::shared_ptr<StrictMock<api::dbus::v1::test::MockPackageManager>> pkgMan;
    std::unique_ptr<StrictMock<MockPrinter>> printer;
    std::unique_ptr<Cli> cli;

    static void SetUpTestSuite() { registerDBusParam(); }

    void SetUp() override
    {
        printer = std::make_unique<StrictMock<MockPrinter>>();
        appMan = std::make_unique<StrictMock<MockAppManager>>();
        pkgMan = std::make_shared<StrictMock<api::dbus::v1::test::MockPackageManager>>(
          "org.deepin.linglong.AppManager",
          "/org/deepin/linglong/AppManager",
          QDBusConnection::sessionBus());

        cli = std::make_unique<Cli>(*printer, *appMan, pkgMan);
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

    EXPECT_CALL(*appMan, Run)
      .Times(1)
      .WillOnce([](const linglong::service::RunParamOption &paramOption)
                  -> linglong::utils::error::Result<void> {
          return LINGLONG_OK;
      });

    auto ret = cli->run(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Exec)
{
    auto args = parseCommand("ll-cli exec com.163.music ls");
    EXPECT_CALL(*appMan, Exec).Times(1).WillOnce(Return(service::Reply{ 0, "" }));

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
      .WillOnce(Return(service::QueryReply{ { 0, "" }, "[]" }));
    EXPECT_CALL(*printer, printContainers);

    auto ret = cli->ps(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Kill)
{
    auto args = parseCommand("ll-cli kill xxxx");

    EXPECT_CALL(*appMan, Stop)
      .Times(1)
      .WillOnce(Return(service::Reply{ STATUS_CODE(kErrorPkgKillSuccess), "" }));

    auto ret = cli->kill(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Install)
{
    GTEST_SKIP() << "skip install test for now";
    auto args = parseCommand("ll-cli install \'xxxx\'");
    EXPECT_CALL(*pkgMan, Install).Times(1).WillOnce(Return(createReply(service::Reply{ 0, "" })));

    auto ret = cli->install(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, Upgrade)
{
    GTEST_SKIP() << "skip upgrade test for now";
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
        createReply(service::QueryReply{ { STATUS_CODE(kErrorPkgQuerySuccess), "" }, R"([])" })));
    EXPECT_CALL(*printer, printErr).Times(1);

    auto ret = cli->search(args);

    EXPECT_EQ(ret, -1);
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
    EXPECT_CALL(*printer, printAppMetaInfos).Times(1);

    auto ret = cli->list(args);

    EXPECT_EQ(ret, 0);
}

TEST_F(CLITest, repo)
{
    auto args = parseCommand("ll-cli repo list");
    EXPECT_CALL(*pkgMan, getRepoInfo)
      .Times(1)
      .WillOnce(
        Return(createReply(service::QueryReply{ { STATUS_CODE(kErrorPkgQuerySuccess), "" }, "" })));

    auto ret = cli->repo(args);

    EXPECT_EQ(ret, 0);
}

} // namespace linglong::cli::test
