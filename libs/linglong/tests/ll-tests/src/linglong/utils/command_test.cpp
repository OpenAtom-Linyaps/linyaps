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
    MOCK_METHOD(linglong::utils::error::Result<QString>,
                exec,
                (const QStringList &args),
                (noexcept));
    MOCK_METHOD(linglong::utils::error::Result<bool>, exists, (), (noexcept));
};

TEST(command, Exec)
{
    auto ret = linglong::utils::command::Cmd("echo").exec({ "-n", "hello" });
    EXPECT_TRUE(ret);
    EXPECT_EQ(ret->length(), 5);
    EXPECT_EQ(ret->toStdString(), "hello");
    auto ret2 = linglong::utils::command::Cmd("id").exec();
    EXPECT_TRUE(ret2.has_value());
    // 获取当前用户名
    auto user = getenv("USER");
    EXPECT_TRUE(user);
    // 输出包含用户名
    EXPECT_TRUE(ret2->toStdString().find(user) != std::string::npos);

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
