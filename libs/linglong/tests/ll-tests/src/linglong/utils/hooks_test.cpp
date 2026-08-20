// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/hooks.h"

namespace linglong::utils::details {

TEST(InstallHooks, ParseUnquotedCommand)
{
    auto command = parseInstallHookCommandLine("ll-pre-install=echo ok  ", "ll-pre-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, "echo ok  ");
}

TEST(InstallHooks, ParseDoubleQuotedCommand)
{
    auto command = parseInstallHookCommandLine(R"(ll-post-install="echo ok")", "ll-post-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, "echo ok");
}

TEST(InstallHooks, ParseSingleQuotedCommand)
{
    auto command = parseInstallHookCommandLine("ll-post-uninstall='echo ok'", "ll-post-uninstall=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, "echo ok");
}

TEST(InstallHooks, KeepInnerQuotesAndBackslashes)
{
    auto command =
      parseInstallHookCommandLine(R"(ll-post-install="echo hello\")", "ll-post-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, R"(echo hello\)");
}

TEST(InstallHooks, KeepMixedInnerQuotes)
{
    auto command =
      parseInstallHookCommandLine(R"(ll-post-install="printf '%s\n' \"ok\"")", "ll-post-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, R"(printf '%s\n' \"ok\")");
}

TEST(InstallHooks, KeepSemicolonInsideQuotedCommand)
{
    auto command =
      parseInstallHookCommandLine(R"(  ll-post-install="echo ok;"  )", "ll-post-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    ASSERT_TRUE(command->has_value());
    EXPECT_EQ(**command, "echo ok;");
}

TEST(InstallHooks, RejectTrailingSemicolonAfterOuterQuote)
{
    auto command = parseInstallHookCommandLine(R"(ll-post-install="echo ok";)", "ll-post-install=");

    EXPECT_FALSE(command.has_value());
}

TEST(InstallHooks, IgnoreLineWithoutPrefixAtStart)
{
    auto command =
      parseInstallHookCommandLine(R"(# ll-post-install="echo ok")", "ll-post-install=");

    ASSERT_TRUE(command.has_value()) << command.error().message();
    EXPECT_FALSE(command->has_value());
}

TEST(InstallHooks, RejectUnterminatedOuterQuote)
{
    auto command = parseInstallHookCommandLine(R"(ll-post-install="echo ok)", "ll-post-install=");

    EXPECT_FALSE(command.has_value());
}

} // namespace linglong::utils::details

namespace linglong::utils {

TEST(InstallHookManagerTest, EmptyHooksAreNoop)
{
    InstallHookManager manager;
    EXPECT_TRUE(manager.executeInstallHooks("/tmp/test.uab").has_value());
    EXPECT_TRUE(manager.executePostInstallHooks("org.deepin.demo", "/path").has_value());
    EXPECT_TRUE(manager.executePostUninstallHooks("org.deepin.demo").has_value());
}

TEST(InstallHookManagerTest, ParseInstallHooksMissingDirYieldsNoHooks)
{
    // When LINGLONG_INSTALL_HOOKS_DIR does not exist, the directory iterator
    // is empty and parseInstallHooks simply produces no commands.
    InstallHookManager manager;
    auto result = manager.parseInstallHooks();
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

} // namespace linglong::utils
