/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../mocks/linglong_builder_mock.h"
#include "common/tempdir.h"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/linglong_builder.h"
#include "linglong/package/layer_dir.h"
#include "linglong/package/reference.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace linglong;
using ::testing::_;
using ::testing::IsEmpty;
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

api::types::v1::RepoConfigV2 makeRepoConfig()
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

std::optional<package::Reference> importLayer(repo::OSTreeRepo &repo,
                                              const std::filesystem::path &workspace,
                                              const std::string &id,
                                              const std::string &version,
                                              const std::string &module)
{
    const auto layerDir = workspace / (id + "-" + module);
    std::filesystem::create_directories(layerDir);

    api::types::v1::PackageInfoV2 info;
    info.arch = { "x86_64" };
    info.channel = "stable";
    info.command = std::vector<std::string>{ "echo", id };
    info.id = id;
    info.kind = "app";
    info.packageInfoV2Module = module;
    info.version = version;
    std::ofstream(layerDir / "info.json") << nlohmann::json(info).dump();

    auto imported = repo.importLayerDir(package::LayerDir{ layerDir });
    if (!imported) {
        ADD_FAILURE() << "failed to import layer: " << imported.error().message();
        return std::nullopt;
    }

    auto ref = package::Reference::parse("stable:" + id + "/" + version + "/x86_64");
    if (!ref) {
        ADD_FAILURE() << ref.error().message();
        return std::nullopt;
    }

    return *ref;
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
    static api::types::v1::RepoConfigV2 makeConfig() { return makeRepoConfig(); }
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
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_EQ(capture.str(),
              "main:org.example.a/1.0.0.1/x86_64\n"
              "main:org.example.b/1.0.0.1/x86_64\n");
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

TEST(BuilderCommands, CmdRemoveAppWithValidRefRemovesLayerAndClearsModules)
{
    TempDir dir;
    const auto repoDir = dir.path() / "repo";
    std::filesystem::create_directories(repoDir);

    auto created = repo::OSTreeRepo::create(repoDir, makeRepoConfig());
    ASSERT_TRUE(created.has_value()) << created.error().message();
    auto repo = std::move(*created);

    auto ref =
      importLayer(*repo, dir.path() / "layer-sources", "org.example.app", "1.0.0.1", "binary");
    ASSERT_TRUE(ref.has_value()) << "failed to import layer fixture";

    auto before = repo->getLayerItem(*ref, "binary");
    ASSERT_TRUE(before.has_value()) << before.error().message();

    auto result = linglong::builder::cmdRemoveApp(*repo, { ref->toString() }, true);
    ASSERT_TRUE(result.has_value()) << result.error().message();

    EXPECT_FALSE(repo->getLayerItem(*ref, "binary").has_value());
    EXPECT_THAT(repo->getModuleList(*ref), IsEmpty());
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
