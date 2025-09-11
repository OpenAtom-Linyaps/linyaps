// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/utils/command/cmd.h"
#include "linglong/utils/error/error.h"

class MockCmd : public linglong::utils::command::Cmd
{
public:
    MockCmd(const QString &command)
        : Cmd(command)
    {
    }

    MOCK_METHOD(linglong::utils::error::Result<QString>, exec, (const QStringList &), (const));
    MOCK_METHOD(linglong::utils::error::Result<bool>, exists, (), (noexcept));
};

TEST(command, Exec)
{
    auto ret = linglong::utils::command::Cmd("echo").exec({ "-n", "hello" });
    EXPECT_TRUE(ret);
    EXPECT_EQ(ret->length(), 5);
    EXPECT_EQ(ret->toStdString(), "hello");
    auto ret2 = linglong::utils::command::Cmd("id").exec({ "-u" });
    EXPECT_TRUE(ret2.has_value());

    auto userId = ret2->toStdString();
    userId.erase(std::remove(userId.begin(), userId.end(), '\n'), userId.end());
    EXPECT_EQ(userId, std::to_string(getuid()));

    // 测试command不存在时
    auto ret3 = linglong::utils::command::Cmd("nonexistent").exec();
    EXPECT_FALSE(ret3.has_value());

    // 测试exec出错时
    auto ret4 = linglong::utils::command::Cmd("ls").exec({ "nonexistent" });
    EXPECT_FALSE(ret4.has_value());
}

TEST(command, commandExists)
{
    auto ret = linglong::utils::command::Cmd("ls").exists();
    EXPECT_TRUE(ret.has_value()) << ret.error().message().toStdString();
    EXPECT_TRUE(*ret) << "ls command should exist";
    ret = linglong::utils::command::Cmd("nonexistent").exists();
    EXPECT_TRUE(ret.has_value()) << ret.error().message().toStdString();
    EXPECT_FALSE(*ret) << "nonexistent should not exist";
}

TEST(command, setEnv)
{
    linglong::utils::command::Cmd cmd("bash");
    cmd.setEnv("LINGLONG_TEST_SETENV", "OK");
    auto ret = cmd.exec({ "-c", "export" });
    EXPECT_TRUE(ret.has_value()) << ret.error().message().toStdString();
    auto retStr = *ret;
    EXPECT_TRUE(retStr.contains("LINGLONG_TEST_SETENV=")) << retStr.toStdString();
}