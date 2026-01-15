// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/builder/source_fetcher.h"
#include "linglong/utils/error/error.h"

#include <QDir>

using namespace linglong;

namespace linglong::builder {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::Return;
using ::testing::ReturnRef;

class MockCommand : public linglong::utils::Cmd
{
public:
    MockCommand(std::string command)
        : Cmd(command)
    {
    }

    MOCK_METHOD(utils::error::Result<std::string>,
                exec,
                (const std::vector<std::string> &args),
                (noexcept, override));
    MOCK_METHOD(bool, exists, (), (noexcept, override));
    MOCK_METHOD(Cmd &,
                setEnv,
                (const std::string &name, const std::string &value),
                (noexcept, override));
};

class SourceFetcherTest : public ::testing::Test
{
protected:
    void SetUp() override { }
};

// 测试获取文件类型源码的正常流程
// 场景：提供有效的文件类型source配置，包括url、digest和name
// 预期：成功获取源码并返回ok
TEST_F(SourceFetcherTest, FetchFileSource)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.digest = "abcdef123456";
    source.name = "repo";

    QDir cacheDir("/tmp/cache");
    auto mockCmd = std::make_shared<MockCommand>("mock");
    EXPECT_CALL(*mockCmd,
                exec(ElementsAre(EndsWith("fetch-file-source"),
                                 "/tmp/dest/" + source.name.value(),
                                 source.url,
                                 source.digest,
                                 "/tmp/cache")))
      .WillOnce(Return("ok"));
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value()) << ret.error().message();
}

// 测试获取git类型源码的正常流程
// 场景：提供有效的git类型source配置，包括url、commit和name
// 预期：成功获取源码并返回ok
TEST_F(SourceFetcherTest, FetchGitSource)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/repo.git";
    source.commit = "abcdef123456";
    source.name = "repo";

    QDir cacheDir("/tmp/cache");
    auto mockCmd = std::make_shared<MockCommand>("mock");
    EXPECT_CALL(*mockCmd,
                exec(ElementsAre(EndsWith("fetch-git-source"),
                                 "/tmp/dest/" + source.name.value(),
                                 source.url,
                                 source.commit,
                                 "/tmp/cache")))
      .WillOnce(Return("ok"));
    EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true")).WillOnce(ReturnRef(*mockCmd));
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value()) << ret.error().message();
}

// 测试git子模块处理功能
// 场景1：默认启用子模块时，验证环境变量设置正确
// 场景2：手动禁用子模块时，验证环境变量被清空
// 预期：两种情况下都能成功获取源码
TEST_F(SourceFetcherTest, gitSubmodules)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/repo.git";
    source.commit = "abcdef123456";
    source.name = "repo";
    // default is true
    {
        QDir cacheDir("/tmp/cache");
        auto mockCmd = std::make_shared<MockCommand>("mock");
        EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return("ok"));
        EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true")).WillOnce(ReturnRef(*mockCmd));
        SourceFetcher fetcher(source, cacheDir);
        fetcher.setCommand(mockCmd);
        auto ret2 = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_TRUE(ret2.has_value()) << ret2.error().message();
    }
    // manually disable submodules
    source.submodules = false;
    {
        QDir cacheDir("/tmp/cache");
        auto mockCmd = std::make_shared<MockCommand>("mock");
        EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return("ok"));
        EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "")).WillOnce(ReturnRef(*mockCmd));
        SourceFetcher fetcher(source, cacheDir);
        fetcher.setCommand(mockCmd);
        auto ret2 = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_TRUE(ret2.has_value()) << ret2.error().message();
    }
}

// 测试无效的source类型
// 场景：提供无效的source.kind值
// 预期：返回错误结果，错误码为-1
TEST_F(SourceFetcherTest, FetchInvalidSourceKind)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "invalid";
    source.url = "https://example.com/repo.git";
    source.name = "repo";

    QDir cacheDir("/tmp/cache");
    auto mockCmd = std::make_shared<MockCommand>("mock");
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -1);
}

// 测试缺失必填字段的情况
// 场景：缺少url和name字段
// 预期：返回错误结果，错误码为-1
TEST_F(SourceFetcherTest, FetchMissingRequiredField)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    // 缺少 url 和 name
    source.commit = "abcdef123456";

    QDir cacheDir("/tmp/cache");
    auto mockCmd = std::make_shared<MockCommand>("mock");
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -1);
}

// 测试无效的git commit
// 场景1：未设置commit字段
// 场景2：设置无效的commit值
// 预期：两种情况都返回错误结果，场景2的错误码为-2
TEST_F(SourceFetcherTest, FetchInvalidGitCommit)
{
    LINGLONG_TRACE("FetchInvalidGitCommit");

    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/repo.git";
    source.name = "repo";

    QDir cacheDir("/tmp/cache");
    {
        SourceFetcher fetcher(source, cacheDir);
        auto ret = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_FALSE(ret.has_value());
    }
    source.commit = "invalid_commit";
    SourceFetcher fetcher(source, cacheDir);
    auto mockCmd = std::make_shared<MockCommand>("mock");
    EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return(LINGLONG_ERR("Invalid commit", -2)));
    EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true")).WillOnce(ReturnRef(*mockCmd));
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -2);
}

// 测试无效的文件digest
// 场景1：未设置digest字段
// 场景2：设置无效的digest值
// 预期：两种情况都返回错误结果，场景2的错误码为-3
TEST_F(SourceFetcherTest, FetchInvalidFileDigest)
{
    LINGLONG_TRACE("FetchInvalidFileDigest");

    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.name = "repo";
    QDir cacheDir("/tmp/cache");
    {
        SourceFetcher fetcher(source, cacheDir);
        auto ret = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_FALSE(ret.has_value());
    }

    source.digest = "invalid_digest";
    SourceFetcher fetcher(source, cacheDir);
    auto mockCmd = std::make_shared<MockCommand>("mock");
    EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return(LINGLONG_ERR("Invalid digest", -3)));
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -3);
}

// 测试未设置name字段的情况
// 场景：仅设置url和digest字段
// 预期：仍能成功获取源码
TEST_F(SourceFetcherTest, FetchNoSetName)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.digest = "digest";

    QDir cacheDir("/tmp/cache");
    auto mockCmd = std::make_shared<MockCommand>("mock");
    EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return("ok"));
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value());
}

} // namespace linglong::builder
