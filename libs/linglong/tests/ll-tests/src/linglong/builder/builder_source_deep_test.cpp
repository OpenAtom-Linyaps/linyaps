/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "linglong/builder/source_fetcher.h"
#include "linglong/builder/config.h"
#include "linglong/builder/printer.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <vector>

namespace linglong::builder {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::Return;
using ::testing::ReturnRef;

class MockCommandDeep : public linglong::utils::Cmd
{
public:
    explicit MockCommandDeep(std::string command)
        : Cmd(std::move(command))
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

class BuilderSourceDeepTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(tempDir.isValid());
        destDir = QDir(tempDir.path() + "/dest");
        cacheDir = QDir(tempDir.path() + "/cache");
        destDir.mkpath(".");
        cacheDir.mkpath(".");
    }

    QTemporaryDir tempDir;
    QDir destDir;
    QDir cacheDir;
};

TEST_F(BuilderSourceDeepTest, FetchArchiveSourceWithCompressionType)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "archive";
    source.url = "https://example.com/archive.tar.gz";
    source.digest = "sha256:1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";
    source.name = "archive_mod";

    auto mockCmd = std::make_shared<MockCommandDeep>("mock-archive");
    EXPECT_CALL(*mockCmd, exec(_))
      .WillOnce(Return("ok"));

    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_TRUE(res.has_value()) << res.error().message();
}

TEST_F(BuilderSourceDeepTest, FetchPatchSourceValidationAndExecution)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "patch";
    source.url = "https://example.com/fixes.patch";
    source.digest = "sha256:fedcba0987654321fedcba0987654321fedcba0987654321fedcba0987654321";
    source.name = "patch_step";

    auto mockCmd = std::make_shared<MockCommandDeep>("mock-patch");
    EXPECT_CALL(*mockCmd, exec(_))
      .WillOnce(Return("ok"));

    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_TRUE(res.has_value()) << res.error().message();
}

TEST_F(BuilderSourceDeepTest, FetchGitSourceWithBranchAndTagRef)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://github.com/example/project.git";
    source.branch = "main";
    source.commit = "a1b2c3d4e5f67890123456789012345678901234";
    source.name = "git_checkout";

    auto mockCmd = std::make_shared<MockCommandDeep>("mock-git-ref");
    EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true"))
      .WillOnce(ReturnRef(*mockCmd));
    EXPECT_CALL(*mockCmd, exec(_))
      .WillOnce(Return("ok"));

    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_TRUE(res.has_value()) << res.error().message();
}

TEST_F(BuilderSourceDeepTest, FetchSourceHandlesCommandFailureAndErrorPropagation)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/failing.git";
    source.commit = "1111222233334444555566667777888899990000";
    source.name = "fail_mod";

    auto mockCmd = std::make_shared<MockCommandDeep>("mock-fail");
    EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true"))
      .WillOnce(ReturnRef(*mockCmd));
    EXPECT_CALL(*mockCmd, exec(_))
      .WillOnce(Return(LINGLONG_ERR("network unreachable", -101)));

    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error().code(), -101);
}

TEST_F(BuilderSourceDeepTest, FetchMultipleSourcesInSequence)
{
    std::vector<api::types::v1::BuilderProjectSource> sources;

    api::types::v1::BuilderProjectSource s1;
    s1.kind = "file";
    s1.url = "https://example.com/file1.txt";
    s1.digest = "sha256:1111111111111111111111111111111111111111111111111111111111111111";
    s1.name = "f1";
    sources.push_back(s1);

    api::types::v1::BuilderProjectSource s2;
    s2.kind = "file";
    s2.url = "https://example.com/file2.txt";
    s2.digest = "sha256:2222222222222222222222222222222222222222222222222222222222222222";
    s2.name = "f2";
    sources.push_back(s2);

    for (const auto &src : sources) {
        auto mockCmd = std::make_shared<MockCommandDeep>("mock-seq");
        EXPECT_CALL(*mockCmd, exec(_)).WillOnce(Return("ok"));

        SourceFetcher fetcher(src, cacheDir);
        fetcher.setCommand(mockCmd);

        auto res = fetcher.fetch(destDir);
        EXPECT_TRUE(res.has_value()) << res.error().message();
    }
}

TEST_F(BuilderSourceDeepTest, BuilderConfigYamlParsingAndDefaultFallbacks)
{
    std::string yamlContent = R"(
package:
  id: com.example.demo
  name: demo
  version: 1.0.0
  kind: app
  description: a test package

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.0

command:
  - demo

sources:
  - kind: git
    url: https://github.com/example/demo.git
    commit: deadbeefdeadbeefdeadbeefdeadbeefdeadbeef
    name: demo-src
)";

    QString configPath = tempDir.path() + "/linglong.yaml";
    QFile file(configPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(yamlContent.c_str(), yamlContent.size());
    file.close();

    auto configRes = Config::load(configPath.toStdString());
    ASSERT_TRUE(configRes.has_value()) << configRes.error().message();

    const auto &config = configRes.value();
    EXPECT_EQ(config.package.id, "com.example.demo");
    EXPECT_EQ(config.package.version, "1.0.0");
    EXPECT_EQ(config.sources.size(), 1U);
    EXPECT_EQ(config.sources[0].kind, "git");
}

TEST_F(BuilderSourceDeepTest, BuilderConfigInvalidYamlRejection)
{
    std::string badYaml = R"(
package:
  id: com.example.broken
  version: [invalid yaml format...
)";

    QString configPath = tempDir.path() + "/broken.yaml";
    QFile file(configPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(badYaml.c_str(), badYaml.size());
    file.close();

    auto configRes = Config::load(configPath.toStdString());
    EXPECT_FALSE(configRes.has_value());
}

TEST_F(BuilderSourceDeepTest, EmptyOrMalformedDigestDetection)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/empty.bin";
    source.digest = "";
    source.name = "empty_digest";

    auto mockCmd = std::make_shared<MockCommandDeep>("mock-empty-digest");
    SourceFetcher fetcher(source, cacheDir);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_FALSE(res.has_value());
}

TEST_F(BuilderSourceDeepTest, ConfigValidationWithComplexBuildOptions)
{
    std::string complexYaml = R"(
package:
  id: com.deepin.complexapp
  name: complexapp
  version: 2.1.0-alpha
  kind: app
  description: complex build config test

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.0

build:
  kind: cmake
  manual:
    - cmake -B build -DCMAKE_BUILD_TYPE=Release
    - cmake --build build --parallel
    - cmake --install build --prefix /project/bin

sources:
  - kind: archive
    url: https://example.com/source.tar.xz
    digest: sha256:8888888888888888888888888888888888888888888888888888888888888888
    name: main-src
)";

    QString configPath = tempDir.path() + "/complex.yaml";
    QFile file(configPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(complexYaml.c_str(), complexYaml.size());
    file.close();

    auto configRes = Config::load(configPath.toStdString());
    ASSERT_TRUE(configRes.has_value()) << configRes.error().message();

    const auto &config = configRes.value();
    EXPECT_EQ(config.package.id, "com.deepin.complexapp");
    EXPECT_EQ(config.package.version, "2.1.0-alpha");
}

TEST_F(BuilderSourceDeepTest, SourceFetcherCacheDirectoryPathValidation)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/cache-path-test.git";
    source.commit = "1234567890123456789012345678901234567890";
    source.name = "cache_path";

    QDir nonExistentCache(tempDir.path() + "/non_existent_cache");
    auto mockCmd = std::make_shared<MockCommandDeep>("mock-cache-path");
    EXPECT_CALL(*mockCmd, setEnv("GIT_SUBMODULES", "true"))
      .WillOnce(ReturnRef(*mockCmd));
    EXPECT_CALL(*mockCmd, exec(_))
      .WillOnce(Return("ok"));

    SourceFetcher fetcher(source, nonExistentCache);
    fetcher.setCommand(mockCmd);

    auto res = fetcher.fetch(destDir);
    EXPECT_TRUE(res.has_value()) << res.error().message();
}

TEST_F(BuilderSourceDeepTest, ConfigMissingPackageMetadataHandling)
{
    std::string incompleteYaml = R"(
base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.0
)";

    QString configPath = tempDir.path() + "/incomplete.yaml";
    QFile file(configPath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(incompleteYaml.c_str(), incompleteYaml.size());
    file.close();

    auto configRes = Config::load(configPath.toStdString());
    EXPECT_FALSE(configRes.has_value());
}

} // namespace linglong::builder
