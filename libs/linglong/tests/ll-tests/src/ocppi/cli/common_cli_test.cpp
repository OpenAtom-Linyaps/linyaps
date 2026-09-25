/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "ocppi/cli/CommandFailedError.hpp"
#include "ocppi/cli/crun/Crun.hpp"
#include "ocppi/runtime/ListOption.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

TEST(CommonCLITest, ListForcesJsonOutput)
{
    TempDir tempDir("ocppi-common-cli-");

    const auto runtimePath = tempDir.path() / "runtime";
    {
        std::ofstream runtime(runtimePath);
        ASSERT_TRUE(runtime.is_open());
        runtime << "#!/bin/sh\n"
                   "printf '%s\\n' \"$@\" > \"$0.args\"\n"
                   "printf '[]\\n'\n";
    }
    std::filesystem::permissions(runtimePath,
                                 std::filesystem::perms::owner_read
                                   | std::filesystem::perms::owner_write
                                   | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);

    auto cli = ocppi::cli::crun::Crun::New(runtimePath);
    ASSERT_TRUE(cli.has_value());

    ocppi::runtime::ListOption option{};
    option.format = ocppi::runtime::ListOption::OutputFormat::Text;
    option.root = tempDir.path() / "root";
    option.extra.emplace_back("--quiet");
    auto containers = cli.value()->list(option);
    ASSERT_TRUE(containers.has_value());
    EXPECT_TRUE(containers.value().empty());

    std::ifstream argumentsFile(runtimePath.string() + ".args");
    ASSERT_TRUE(argumentsFile.is_open());
    std::vector<std::string> arguments;
    for (std::string argument; std::getline(argumentsFile, argument);) {
        arguments.emplace_back(argument);
    }
    const std::vector<std::string> expectedArguments{
        "--root", option.root->string(), "list", "--quiet", "-f", "json",
    };
    EXPECT_EQ(arguments, expectedArguments);
}

class CommonCLIExitStatusTest : public ::testing::TestWithParam<std::pair<const char *, int>>
{
};

TEST_P(CommonCLIExitStatusTest, PreservesCompletedCommandStatus)
{
    TempDir tempDir("ocppi-exit-status-");
    const auto runtimePath = tempDir.path() / "runtime";
    {
        std::ofstream runtime(runtimePath);
        ASSERT_TRUE(runtime.is_open());
        runtime << "#!/bin/sh\n" << GetParam().first << '\n';
    }
    std::filesystem::permissions(runtimePath, std::filesystem::perms::owner_all);
    auto cli = ocppi::cli::crun::Crun::New(runtimePath);
    ASSERT_TRUE(cli.has_value());

    auto result = cli.value()->exec("test-container", "/bin/true", {});
    if (GetParam().second == 0) {
        EXPECT_TRUE(result.has_value());
        return;
    }
    ASSERT_FALSE(result.has_value());
    try {
        std::rethrow_exception(result.error());
    } catch (const ocppi::cli::CommandFailedError &error) {
        EXPECT_EQ(error.exitStatus(), GetParam().second);
        EXPECT_NE(std::string(error.what()).find("retval="), std::string::npos);
    } catch (...) {
        FAIL() << "Expected CommandFailedError for a completed runtime command";
    }
}

INSTANTIATE_TEST_SUITE_P(RuntimeCommand,
                         CommonCLIExitStatusTest,
                         ::testing::Values(std::make_pair("exit 0", 0),
                                           std::make_pair("exit 1", 1),
                                           std::make_pair("exit 7", 7),
                                           std::make_pair("exit 42", 42),
                                           std::make_pair("exit 126", 126),
                                           std::make_pair("exit 127", 127),
                                           std::make_pair("exit 255", 255),
                                           std::make_pair("kill -TERM $$", 128 + SIGTERM),
                                           std::make_pair("kill -KILL $$", 128 + SIGKILL)));

TEST(CommandFailedErrorTest, MessageOnlyErrorUsesGenericFailure)
{
    const ocppi::cli::CommandFailedError error("runtime did not provide a wait status");
    EXPECT_EQ(error.exitStatus(), EXIT_FAILURE);
}
