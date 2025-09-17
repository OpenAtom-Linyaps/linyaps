// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "../mocks/command_mock.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/source_fetcher.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QProcessEnvironment>

using namespace linglong;
using namespace testing;

namespace linglong::builder {
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
    mockCmd->warpExecFunc = [&](const QStringList &args) {
        auto argsStr = args.join(" ").toStdString();
        EXPECT_TRUE(args[0].contains("fetch-file-source")) << argsStr;
        EXPECT_EQ(args[1].toStdString(), "/tmp/dest/" + source.name.value()) << argsStr;
        EXPECT_EQ(args[2].toStdString(), source.url) << argsStr;
        EXPECT_EQ(args[3].toStdString(), source.digest) << argsStr;
        EXPECT_EQ(args[4].toStdString(), "/tmp/cache") << argsStr;
        return utils::error::Result<QString>("ok");
    };
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value()) << ret.error().message().toStdString();
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
    mockCmd->warpExecFunc = [&](const QStringList &args) {
        auto argsStr = args.join(" ").toStdString();
        EXPECT_TRUE(args[0].contains("fetch-git-source")) << argsStr;
        EXPECT_EQ(args[1].toStdString(), "/tmp/dest/" + source.name.value()) << argsStr;
        EXPECT_EQ(args[2].toStdString(), source.url) << argsStr;
        EXPECT_EQ(args[3].toStdString(), source.commit) << argsStr;
        EXPECT_EQ(args[4], "/tmp/cache") << argsStr;
        return utils::error::Result<QString>("ok");
    };
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value()) << ret.error().message().toStdString();
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
        mockCmd->warpExecFunc = [&](const QStringList &args) {
            return utils::error::Result<QString>("ok");
        };
        mockCmd->warpSetEnvFunc = [&](const QString &name,
                                      const QString &value) -> linglong::utils::command::Cmd & {
            EXPECT_EQ(name, "GIT_SUBMODULES");
            EXPECT_EQ(value, "true");
            return *mockCmd;
        };
        SourceFetcher fetcher(source, cacheDir);
        fetcher.setCommand(mockCmd);
        auto ret2 = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_TRUE(ret2.has_value()) << ret2.error().message().toStdString();
    }
    // manually disable submodules
    source.submodules = false;
    {
        QDir cacheDir("/tmp/cache");
        auto mockCmd = std::make_shared<MockCommand>("mock");
        mockCmd->warpExecFunc = [&](const QStringList &args) {
            return utils::error::Result<QString>("ok");
        };
        mockCmd->warpSetEnvFunc = [&](const QString &name,
                                      const QString &value) -> linglong::utils::command::Cmd & {
            EXPECT_TRUE(value.isEmpty()) << name.toStdString();
            return *mockCmd;
        };
        SourceFetcher fetcher(source, cacheDir);
        fetcher.setCommand(mockCmd);
        auto ret2 = fetcher.fetch(QDir("/tmp/dest"));
        EXPECT_TRUE(ret2.has_value()) << ret2.error().message().toStdString();
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
    mockCmd->warpExecFunc = [&](const QStringList &args) {
        LINGLONG_TRACE("FetchInvalidGitCommit");
        return LINGLONG_ERR("Invalid commit", -2);
    };
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
    mockCmd->warpExecFunc = [&](const QStringList &args) {
        LINGLONG_TRACE("FetchInvalidFileDigest");
        return LINGLONG_ERR("Invalid digest", -3);
    };
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
    mockCmd->warpExecFunc = [&](const QStringList &args) {
        return utils::error::Result<QString>("ok");
    };
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);
    auto ret = fetcher.fetch(QDir("/tmp/dest"));
    EXPECT_TRUE(ret.has_value());
}

} // namespace linglong::builder
