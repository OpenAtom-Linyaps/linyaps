// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/utils/cmd.h"

namespace {

TEST(command, Exec)
{
    auto ret = linglong::utils::Cmd("echo").exec({ "-n", "hello" });
    EXPECT_TRUE(ret);
    EXPECT_EQ(ret->size(), 5);
    EXPECT_EQ(*ret, "hello");
    auto userId = linglong::utils::Cmd("id").exec({ "-u" });
    EXPECT_TRUE(userId.has_value());

    userId = userId->substr(0, userId->find('\n'));
    EXPECT_EQ(*userId, std::to_string(getuid()));

    // 测试command不存在时
    auto ret3 = linglong::utils::Cmd("nonexistent").exec();
    EXPECT_FALSE(ret3.has_value());

    // 测试exec出错时
    auto ret4 = linglong::utils::Cmd("ls").exec({ "/linglong/nonexistent" });
    EXPECT_FALSE(ret4.has_value());
}

TEST(command, setEnv)
{
    linglong::utils::Cmd cmd("bash");
    // test set
    cmd.setEnv("LINGLONG_TEST_SETENV", "OK");
    auto existsRef = cmd.exists();
    EXPECT_TRUE(existsRef);
    // test unset
    cmd.setEnv("PATH", "");
    auto ret = cmd.exec({ "-c", "export" });
    EXPECT_TRUE(ret.has_value()) << "failed to execute export command: " << ret.error().message();
    EXPECT_TRUE(ret->find("declare -x LINGLONG_TEST_SETENV=") != std::string::npos) << *ret;
    EXPECT_FALSE(ret->find("declare -x PATH=\"") != std::string::npos) << *ret;
}

TEST(command, toStdin)
{
    // Test writing to stdin using cat command
    auto ret = linglong::utils::Cmd("cat").toStdin("Hello, World!").exec();

    EXPECT_TRUE(ret.has_value()) << "failed to execute cat command: " << ret.error().message();
    EXPECT_EQ(*ret, "Hello, World!");

    // Test with multiline input
    auto ret2 = linglong::utils::Cmd("wc").toStdin("line1\nline2\nline3\n").exec({ "-l" });

    EXPECT_TRUE(ret2.has_value()) << "failed to execute wc command: " << ret2.error().message();
    // wc -l should output " 3" for 3 lines
    EXPECT_TRUE(ret2->find("3") != std::string::npos) << *ret2;

    // Test empty stdin
    auto ret3 = linglong::utils::Cmd("cat").toStdin("").exec();

    EXPECT_TRUE(ret3.has_value()) << "failed to execute cat with empty stdin: "
                                  << ret3.error().message();
    EXPECT_TRUE(ret3->empty()) << "cat with empty stdin should return empty output";

    // Test with data exceeding 1M
    std::string large_data(1048576 + 1024, 'A');
    auto ret4 = linglong::utils::Cmd("wc").toStdin(large_data).exec({ "-c" });

    EXPECT_TRUE(ret4.has_value()) << "failed to execute wc with large stdin: "
                                  << ret4.error().message();
    EXPECT_TRUE(ret4->find("1049600") != std::string::npos)
      << "wc -c should report 1049600 bytes, got: " << *ret4;
}

} // namespace
