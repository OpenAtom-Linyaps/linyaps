// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/bash_command_helper.h"

namespace linglong::utils {

TEST(BashCommandHelper, GenerateBashCommandBase)
{
    auto cmd = BashCommandHelper::generateBashCommandBase();
    EXPECT_EQ(cmd.size(), 4);
    EXPECT_EQ(cmd[0], "/bin/bash");
    EXPECT_EQ(cmd[1], "--noprofile");
    EXPECT_EQ(cmd[2], "--norc");
    EXPECT_EQ(cmd[3], "-c");
}

TEST(BashCommandHelper, GenerateDefaultBashCommand)
{
    auto cmd = BashCommandHelper::generateDefaultBashCommand();
    EXPECT_EQ(cmd.size(), 5);
    EXPECT_EQ(cmd[4], "source /etc/profile; bash --norc");
}

TEST(BashCommandHelper, GenerateExecCommand)
{
    auto cmd = BashCommandHelper::generateExecCommand("test-command");
    EXPECT_EQ(cmd.size(), 6);
    EXPECT_EQ(cmd[0], "/run/linglong/container-init");
    EXPECT_EQ(cmd[4], "-c");
    EXPECT_EQ(cmd[5], "test-command");
}

TEST(BashCommandHelper, GenerateEntrypointScript)
{
    std::vector<std::string> args = { "arg1", "arg2 with space", "arg3" };
    auto script = BashCommandHelper::generateEntrypointScript(args);

    EXPECT_EQ(script.find("#!/bin/bash\n"), 0);
    EXPECT_NE(script.find("source /etc/profile\n"), std::string::npos);

    EXPECT_NE(script.find("arg1"), std::string::npos);
    EXPECT_NE(script.find("arg2 with space"), std::string::npos);
    EXPECT_NE(script.find("arg3"), std::string::npos);
    EXPECT_NE(script.find("exec"), std::string::npos);
}

} // namespace linglong::utils
