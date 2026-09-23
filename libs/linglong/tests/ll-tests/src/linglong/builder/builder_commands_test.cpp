/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../mocks/linglong_builder_mock.h"
#include "common/tempdir.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <sstream>
#include <string>
#include <vector>

using namespace linglong;
using ::testing::_;
using ::testing::Return;

namespace {

class CaptureStdout
{
public:
    CaptureStdout()
        : old_(std::cout.rdbuf(stream_.rdbuf()))
    {
    }

    ~CaptureStdout() { std::cout.rdbuf(old_); }

    std::string str() const { return stream_.str(); }

private:
    std::ostringstream stream_;
    std::streambuf *old_;
};

api::types::v1::PackageInfoV2 makePkg(const std::string &id,
                                      const std::string &version,
                                      const std::string &channel = "main")
{
    api::types::v1::PackageInfoV2 pkg;
    pkg.arch = { "x86_64" };
    pkg.channel = channel;
    pkg.id = id;
    pkg.kind = "app";
    pkg.version = version;
    return pkg;
}

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(path, makeConfig())
    {
    }

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::PackageInfoV2>>,
                listLocal,
                (),
                (override, const, noexcept));
    MOCK_METHOD(std::vector<std::string>,
                getModuleList,
                (const package::Reference &ref),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<void>, prune, (), (override));
    MOCK_METHOD(utils::error::Result<void>, mergeModules, (), (override, const, noexcept));

private:
    static api::types::v1::RepoConfigV2 makeConfig()
    {
        api::types::v1::RepoConfigV2 config;
        config.defaultRepo = "stable";
        config.version = 2;
        api::types::v1::Repo repo;
        repo.name = "stable";
        repo.priority = 0;
        repo.url = "https://example.com/repo";
        config.repos.push_back(std::move(repo));
        return config;
    }
};

TEST(BuilderCommands, SetConfigGetConfigRoundTrip)
{
    linglong::builder::BuilderMock builder;

    api::types::v1::BuilderConfig cfg;
    cfg.version = 2;
    builder.setConfig(cfg);

    EXPECT_EQ(builder.getConfig().version, 2);
}

TEST(BuilderCommands, CmdListAppPrintsSortedUniqueRefs)
{
    TempDir dir;
    MockRepo repo(dir.path());

    EXPECT_CALL(repo, listLocal())
      .WillOnce(Return(std::vector<api::types::v1::PackageInfoV2>{
        makePkg("org.example.b", "1.0.0.1"),
        makePkg("org.example.a", "1.0.0.1"),
        makePkg("org.example.a", "1.0.0.1"),
      }));

    CaptureStdout capture;
    auto result = linglong::builder::cmdListApp(repo);
    EXPECT_TRUE(result.has_value());
    auto out = capture.str();

    auto first = out.find("main:org.example.a");
    auto second = out.find("main:org.example.b");
    EXPECT_NE(first, std::string::npos);
    EXPECT_NE(second, std::string::npos);
    EXPECT_LT(first, second);
}

TEST(BuilderCommands, CmdListAppReturnsErrorOnListFailure)
{
    LINGLONG_TRACE("cmd list app");
    TempDir dir;
    MockRepo repo(dir.path());

    EXPECT_CALL(repo, listLocal()).WillOnce(Return(LINGLONG_ERR("list failed")));

    auto result = linglong::builder::cmdListApp(repo);
    EXPECT_FALSE(result.has_value());
}

TEST(BuilderCommands, CmdRemoveAppWithValidRefPrunesAndMerges)
{
    TempDir dir;
    MockRepo repo(dir.path());

    auto ref = package::Reference::parse("main:org.example.app/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    // No modules means no real remove() call; prune and mergeModules are still
    // invoked so the success path is exercised without touching the filesystem.
    const std::vector<std::string> modules{};
    EXPECT_CALL(repo, getModuleList(_)).WillOnce(Return(modules));
    EXPECT_CALL(repo, prune()).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = linglong::builder::cmdRemoveApp(repo, { ref->toString() }, true);
    EXPECT_TRUE(result.has_value());
}

TEST(BuilderCommands, CmdRemoveAppSkipsInvalidRef)
{
    TempDir dir;
    MockRepo repo(dir.path());

    EXPECT_CALL(repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = linglong::builder::cmdRemoveApp(repo, { "not a valid ref" }, false);
    EXPECT_TRUE(result.has_value());
}

} // namespace
