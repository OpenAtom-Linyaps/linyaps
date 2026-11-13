/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/env.h"
#include "linglong/utils/file.h"
#include "linglong/utils/hooks.h"
#include "linglong/utils/namespace.h"
#include "linglong/utils/overlayfs.h"
#include "linglong/utils/terminal/terminal_guard.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <unistd.h>

namespace {

namespace fs = std::filesystem;

TEST(UtilsFileEdge, CalculateDirectorySizeIncludesNestedFiles)
{
    TempDir tempDir;
    const auto root = tempDir.path() / "root";
    fs::create_directories(root / "nested");
    std::ofstream(root / "one.bin") << std::string(5, 'a');
    std::ofstream(root / "nested" / "two.bin") << std::string(7, 'b');

    auto size = linglong::utils::calculateDirectorySize(root);
    ASSERT_TRUE(size) << size.error().message();
    EXPECT_GE(*size, 12U);
}

TEST(UtilsFileEdge, CalculateDirectorySizeReportsMissingDirectory)
{
    TempDir tempDir;
    auto size = linglong::utils::calculateDirectorySize(tempDir.path() / "missing");
    EXPECT_FALSE(size);
    EXPECT_FALSE(size.error().message().empty());
}

TEST(UtilsFileEdge, CalculateDirectorySizeCountsSymlinkItself)
{
    TempDir tempDir;
    std::ofstream(tempDir.path() / "target") << "data";
    fs::create_symlink("target", tempDir.path() / "link");

    auto size = linglong::utils::calculateDirectorySize(tempDir.path());
    ASSERT_TRUE(size) << size.error().message();
    EXPECT_GE(*size, 4U);
}

TEST(UtilsFileEdge, ReadFileReportsMissingFile)
{
    TempDir tempDir;
    auto content = linglong::utils::readFile(tempDir.path() / "missing");
    ASSERT_FALSE(content);
    EXPECT_THAT(content.error().message(), ::testing::HasSubstr("file not found"));
}

TEST(UtilsFileEdge, WriteFileReportsMissingParentDirectory)
{
    TempDir tempDir;
    auto result = linglong::utils::writeFile(tempDir.path() / "missing" / "file.txt", "content");
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.error().message().empty());
}

TEST(UtilsFileEdge, WriteFileOverridesPermissions)
{
    TempDir tempDir;
    const auto path = tempDir.path() / "file.txt";
    auto result = linglong::utils::writeFile(path, "content", fs::perms::owner_read);
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(fs::status(path).permissions() & fs::perms::owner_read, fs::perms::owner_read);
}

TEST(UtilsFileEdge, ConcatFileRejectsMissingSource)
{
    TempDir tempDir;
    auto result = linglong::utils::concatFile(tempDir.path() / "missing", tempDir.path() / "out");
    EXPECT_FALSE(result);
}

TEST(UtilsFileEdge, ConcatFileRejectsSameSourceAndTarget)
{
    TempDir tempDir;
    const auto path = tempDir.path() / "same.txt";
    std::ofstream(path) << "content";

    auto result = linglong::utils::concatFile(path, path);
    EXPECT_FALSE(result);
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("same file"));
}

TEST(UtilsFileEdge, ConcatFileAppendsContent)
{
    TempDir tempDir;
    const auto source = tempDir.path() / "source.txt";
    const auto target = tempDir.path() / "target.txt";
    std::ofstream(source) << "second";
    std::ofstream(target) << "first";

    auto result = linglong::utils::concatFile(source, target);
    ASSERT_TRUE(result) << result.error().message();

    auto content = linglong::utils::readFile(target);
    ASSERT_TRUE(content) << content.error().message();
    EXPECT_EQ(*content, "firstsecond");
}

TEST(UtilsHookEdge, ParseInstallHookCommandLineHandlesSupportedForms)
{
    struct Case
    {
        std::string label;
        std::string line;
        std::string prefix;
        bool expectValue;
        std::string expected;
    };

    const std::vector<Case> cases = {
        { "plain", "ll-pre-install=/bin/true", "ll-pre-install=", true, "/bin/true" },
        { "leading whitespace",
          "   ll-pre-install=  /bin/false",
          "ll-pre-install=",
          true,
          "/bin/false" },
        { "double quoted",
          "ll-post-install=\"echo hello\"",
          "ll-post-install=",
          true,
          "echo hello" },
        { "single quoted", "ll-post-install='echo $HOME'", "ll-post-install=", true, "echo $HOME" },
        { "empty command", "ll-post-install=", "ll-post-install=", true, "" },
        { "different prefix", "ll-post-install=/bin/true", "ll-pre-install=", false, "" },
        { "unrelated line", "#!/bin/sh", "ll-pre-install=", false, "" },
    };

    for (const auto &tc : cases) {
        SCOPED_TRACE(tc.label);
        auto parsed = linglong::utils::details::parseInstallHookCommandLine(tc.line, tc.prefix);
        ASSERT_TRUE(parsed) << parsed.error().message();
        ASSERT_EQ(parsed->has_value(), tc.expectValue);
        if (tc.expectValue) {
            EXPECT_EQ(**parsed, tc.expected);
        }
    }
}

TEST(UtilsHookEdge, ParseInstallHookCommandLineRejectsUnterminatedQuote)
{
    auto parsed =
      linglong::utils::details::parseInstallHookCommandLine("ll-pre-install=\"echo hello",
                                                            "ll-pre-install=");
    EXPECT_FALSE(parsed);
    EXPECT_THAT(parsed.error().message(), ::testing::HasSubstr("unterminated"));
}

TEST(UtilsHookEdge, HookManagerWithoutCommandsIsNoop)
{
    linglong::utils::InstallHookManager manager;
    EXPECT_TRUE(manager.executeInstallHooks("/tmp/unused.layer"));
    EXPECT_TRUE(manager.executePostInstallHooks("org.example.App", "/tmp/unused"));
    EXPECT_TRUE(manager.executePostUninstallHooks("org.example.App"));
}

TEST(UtilsNamespaceEdge, ParseSubuidRangesFiltersByNameAndUid)
{
    std::istringstream input{
        "alice:100000:65536\nbob:200000:65536\n1000:300000:65536\n# comment\n"
    };

    auto byName = linglong::utils::detail::parseSubuidRanges(input, 2000, "alice");
    ASSERT_TRUE(byName) << byName.error().message();
    EXPECT_EQ(byName->size(), 1U);
    EXPECT_EQ(byName->front().subuid, "100000");
    EXPECT_EQ(byName->front().count, "65536");

    std::istringstream uidInput{ "alice:100000:65536\n1000:300000:65536\n" };
    auto byUid = linglong::utils::detail::parseSubuidRanges(uidInput, 1000, "nobody");
    ASSERT_TRUE(byUid) << byUid.error().message();
    ASSERT_EQ(byUid->size(), 1U);
    EXPECT_EQ(byUid->front().subuid, "300000");
}

TEST(UtilsNamespaceEdge, ParseSubuidRangesRejectsMalformedMatchingLine)
{
    std::istringstream input{ "alice:100000\n" };
    auto ranges = linglong::utils::detail::parseSubuidRanges(input, 1000, "alice");
    EXPECT_FALSE(ranges);
    EXPECT_THAT(ranges.error().message(), ::testing::HasSubstr("invalid subuid"));
}

TEST(UtilsNamespaceEdge, GetUserNameReportsUnknownUser)
{
    auto name = linglong::utils::detail::getUserName(static_cast<uid_t>(4294967294U));
    EXPECT_FALSE(name);
    EXPECT_FALSE(name.error().message().empty());
}

TEST(UtilsNamespaceEdge, GetUserNameResolvesCurrentUser)
{
    auto name = linglong::utils::detail::getUserName(::getuid());
    ASSERT_TRUE(name) << name.error().message();
    EXPECT_FALSE(name->empty());
}

TEST(UtilsOverlayEdge, OverlayModeStringsRoundTrip)
{
    using linglong::utils::OverlayMode;
    const std::vector<OverlayMode> modes = { OverlayMode::Auto,
                                             OverlayMode::Kernel,
                                             OverlayMode::FUSE };
    for (auto mode : modes) {
        auto text = linglong::utils::overlayModeToString(mode);
        auto parsed = linglong::utils::overlayModeFromString(text);
        ASSERT_TRUE(parsed) << parsed.error().message();
        EXPECT_EQ(*parsed, mode);
    }
}

TEST(UtilsOverlayEdge, OverlayModeFromStringRejectsUnknownValue)
{
    auto parsed = linglong::utils::overlayModeFromString("binder");
    EXPECT_FALSE(parsed);
    EXPECT_THAT(parsed.error().message(), ::testing::HasSubstr("invalid overlayfs mode"));
}

TEST(UtilsTerminalEdge, TerminalGuardCreateFailsOnNonTerminal)
{
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    auto guard = linglong::utils::terminal::TerminalGuard::create(fds[0]);
    EXPECT_FALSE(guard.has_value());
    ::close(fds[0]);
    ::close(fds[1]);
}

TEST(UtilsTerminalEdge, PropagateWindowSizeFailsOnInvalidDescriptors)
{
    EXPECT_FALSE(linglong::utils::terminal::TerminalGuard::propagateWindowSize(-1, -1));
}

TEST(UtilsCmdEdge, ExecReportsMissingCommand)
{
    linglong::utils::Cmd cmd("linglong-command-that-does-not-exist");
    auto result = cmd.exec();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("command not found"));
}

TEST(UtilsCmdEdge, ExecReportsNonZeroExitAndOutput)
{
    linglong::utils::Cmd cmd("sh");
    auto result = cmd.exec({ "-c", "echo failure; exit 7" });
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("exit code 7"));
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("failure"));
}

TEST(UtilsCmdEdge, ExecReportsSignalTermination)
{
    linglong::utils::Cmd cmd("sh");
    auto result = cmd.exec({ "-c", "kill -TERM $$" });
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(), ::testing::HasSubstr("killed by signal"));
}

TEST(UtilsCmdEdge, ExecRemovesUnsetEnvironmentVariable)
{
    linglong::utils::Cmd cmd("sh");
    cmd.setEnv("LINGLONG_UTILS_UNSET_ME", "");
    auto result = cmd.exec({ "-c", "printf '%s' \"${LINGLONG_UTILS_UNSET_ME:-absent}\"" });
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, "absent");
}

TEST(UtilsCmdEdge, ExecUsesFallbackPathWhenEnvironmentPathIsUnset)
{
    linglong::utils::EnvironmentVariableGuard pathGuard("PATH", "");
    linglong::utils::Cmd cmd("sh");
    auto result = cmd.exec({ "-c", "printf ok" });
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, "ok");
}

TEST(UtilsCmdEdge, ExecFeedsStdinToChild)
{
    linglong::utils::Cmd cmd("sh");
    cmd.toStdin("payload-through-stdin");
    auto result = cmd.exec({ "-c", "cat" });
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, "payload-through-stdin");
}

TEST(UtilsOverlayEdge, AutoModeRefusesMountUntilResolved)
{
    TempDir tempDir;
    const auto lower = tempDir.path() / "lower";
    const auto merged = tempDir.path() / "merged";
    fs::create_directories(lower);
    fs::create_directories(merged);

    linglong::utils::OverlayFS overlay({ lower },
                                       std::nullopt,
                                       std::nullopt,
                                       merged,
                                       linglong::utils::OverlayMode::Auto);
    EXPECT_FALSE(overlay.mount());
    EXPECT_FALSE(overlay.isMounted());
}

TEST(UtilsOverlayEdge, KernelMountFailsWithoutPrivileges)
{
    TempDir tempDir;
    const auto lower = tempDir.path() / "lower";
    const auto upper = tempDir.path() / "upper";
    const auto work = tempDir.path() / "work";
    const auto merged = tempDir.path() / "merged";
    for (const auto &dir : { lower, upper, work, merged }) {
        fs::create_directories(dir);
    }

    linglong::utils::OverlayFS overlay({ lower },
                                       upper,
                                       work,
                                       merged,
                                       linglong::utils::OverlayMode::Kernel);
    EXPECT_FALSE(overlay.mount());
    EXPECT_FALSE(overlay.isMounted());
}

TEST(UtilsOverlayEdge, UnmountIsIdempotentWhenNotMounted)
{
    TempDir tempDir;
    const auto merged = tempDir.path() / "merged";
    fs::create_directories(merged);

    linglong::utils::OverlayFS overlay({ tempDir.path() },
                                       std::nullopt,
                                       std::nullopt,
                                       merged,
                                       linglong::utils::OverlayMode::FUSE);
    overlay.unmount();
    overlay.unmount();
    EXPECT_FALSE(overlay.isMounted());
}

TEST(UtilsNamespaceEdge, RunInNamespaceReportsChildExitStatus)
{
    std::string program = "true";
    std::array<char *, 2> argv{ program.data(), nullptr };
    auto result = linglong::utils::runInNamespace(1, argv.data());
    if (result.has_value()) {
        EXPECT_EQ(*result, 0);
    } else {
        EXPECT_FALSE(result.error().message().empty());
    }
}

TEST(UtilsNamespaceEdge, RunInNamespaceMapsExplicitUserAndGroup)
{
    std::string program = "/bin/sh";
    std::string script = "test \"$(id -u)\" = 12345 && test \"$(id -g)\" = 12345";
    std::array<char *, 4> argv{ program.data(), const_cast<char *>("-c"), script.data(), nullptr };

    auto result = linglong::utils::runInNamespace(3,
                                                  argv.data(),
                                                  static_cast<uid_t>(12345),
                                                  static_cast<gid_t>(12345));
    if (!result) {
        GTEST_SKIP() << "user namespace mapping unavailable: " << result.error().message();
    }
    EXPECT_EQ(*result, 0);
}

TEST(UtilsCmdEdge, ExecOverridesExistingEnvironmentVariable)
{
    linglong::utils::EnvironmentVariableGuard guard("LINGLONG_UTILS_OVERRIDE", "original");
    linglong::utils::Cmd cmd("sh");
    cmd.setEnv("LINGLONG_UTILS_OVERRIDE", "updated");
    auto result = cmd.exec({ "-c", "printf '%s' \"$LINGLONG_UTILS_OVERRIDE\"" });
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, "updated");
}

} // namespace
