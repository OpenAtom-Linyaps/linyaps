// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/hooks.h"

#include <filesystem>
#include <fstream>
#include <string>

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

TEST(InstallHookManagerTest, ExecutesParsedHooksWithExpectedEnvironment)
{
    TempDir tempDir;
    const auto hooksDir = tempDir.path() / "config.d";
    std::filesystem::create_directories(hooksDir);

    const auto hookFile = hooksDir / "ll-tests-hooks.conf";
    std::ofstream out(hookFile);
    ASSERT_TRUE(out.is_open()) << "cannot write install hook file " << hookFile;
    out << "ll-pre-install=test \"$LINGLONG_UAB_PATH\" = /tmp/ll-tests.uab\n"
        << "ll-post-install=test \"$LINGLONG_APPID\" = org.example.Hook && test "
           "\"$LINGLONG_APP_INSTALL_PATH\" = /tmp/ll-tests-app\n"
        << "ll-post-uninstall=test \"$LINGLONG_APPID\" = org.example.Hook\n";
    out.close();

    InstallHookManager manager(hooksDir);
    auto parsed = manager.parseInstallHooks();
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    EXPECT_TRUE(manager.executeInstallHooks("/tmp/ll-tests.uab").has_value());
    EXPECT_TRUE(
      manager.executePostInstallHooks("org.example.Hook", "/tmp/ll-tests-app").has_value());
    EXPECT_TRUE(manager.executePostUninstallHooks("org.example.Hook").has_value());
}

TEST(InstallHookManagerTest, ReportsFailingHookCommand)
{
    TempDir tempDir;
    const auto hooksDir = tempDir.path() / "config.d";
    std::filesystem::create_directories(hooksDir);

    const auto hookFile = hooksDir / "ll-tests-failing-hooks.conf";
    std::ofstream out(hookFile);
    ASSERT_TRUE(out.is_open()) << "cannot write install hook file " << hookFile;
    out << "ll-pre-install=false\n";
    out.close();

    InstallHookManager manager(hooksDir);
    auto parsed = manager.parseInstallHooks();
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();

    auto result = manager.executeInstallHooks("/tmp/ll-tests.uab");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("Hook command 'false' failed"), std::string::npos);
}

} // namespace linglong::utils
