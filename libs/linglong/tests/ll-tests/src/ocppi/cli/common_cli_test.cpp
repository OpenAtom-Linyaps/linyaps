/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "ocppi/cli/crun/Crun.hpp"
#include "ocppi/runtime/ListOption.hpp"
#include "ocppi/types/ContainerListItem.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

TEST(CommonCLITest, ListForcesJsonOutput)
{
    TempDir tempDir("ocppi-common-cli-");
    ASSERT_TRUE(tempDir.isValid());

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

TEST(CommonCLITest, ListFailsWhenRuntimeIsNotExecutable)
{
    TempDir tempDir("ocppi-common-cli-");
    ASSERT_TRUE(tempDir.isValid());

    const auto runtimePath = tempDir.path() / "runtime";
    {
        std::ofstream runtime(runtimePath);
        ASSERT_TRUE(runtime.is_open());
        runtime << "#!/bin/sh\nprintf '[]\\n'\n";
    }
    // Deliberately leave the runtime without any executable permission bit so
    // that execvp fails in the forked child. Crun::New only checks that the
    // binary exists, so constructing the CLI must still succeed.
    std::filesystem::permissions(runtimePath,
                                 std::filesystem::perms::owner_read
                                   | std::filesystem::perms::owner_write,
                                 std::filesystem::perm_options::replace);

    const auto duplicateMarker = tempDir.path() / "duplicate-process-marker";

    auto cli = ocppi::cli::crun::Crun::New(runtimePath);
    ASSERT_TRUE(cli.has_value());

    ocppi::runtime::ListOption option{};
    option.root = tempDir.path() / "root";
    auto containers = cli.value()->list(option);
    EXPECT_FALSE(containers.has_value());

    // A failed exec must terminate the forked child. If the child instead
    // unwound into the caller, it would keep running this test body as a
    // duplicate of the test process and create the marker before this
    // (parent) process gets a chance to write it below.
    EXPECT_FALSE(std::filesystem::exists(duplicateMarker));

    std::ofstream marker(duplicateMarker);
    ASSERT_TRUE(marker.is_open());
}
