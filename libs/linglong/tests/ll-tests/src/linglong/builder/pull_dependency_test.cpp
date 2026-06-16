// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#define private public
#include "linglong/builder/linglong_builder.h"
#undef private
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

using namespace linglong;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace {

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(path, makeConfig())
    {
    }

    MOCK_METHOD(utils::error::Result<package::LayerDir>,
                getLayerDir,
                (const package::Reference &ref,
                 const std::string &module,
                 const std::optional<std::string> &subRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReferenceLocal,
                (const package::FuzzyReference &fuzzy, bool semanticMatching),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                pull,
                (service::Task & taskContext,
                 const package::ReferenceWithRepo &refRepo,
                 const std::string &module),
                (override, noexcept));

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

auto layerCached()
{
    return package::LayerDir("/fake/layer/dir");
}

auto refWithRepo(package::Reference ref, std::string repoName = "stable")
{
    api::types::v1::Repo remoteRepo;
    remoteRepo.name = std::move(repoName);
    remoteRepo.priority = 0;
    remoteRepo.url = "https://example.com/repo";

    return package::ReferenceWithRepo{
        .repo = std::move(remoteRepo),
        .reference = std::move(ref),
    };
}

auto builderProject(std::optional<std::string> runtime = std::nullopt)
{
    api::types::v1::BuilderProject project;
    project.base = "org.deepin.base/23.0.0.0/x86_64";
    project.build = "";
    project.command = std::vector<std::string>{ "true" };
    project.package = api::types::v1::BuilderProjectPackage{
        .architecture = std::string{ "x86_64" },
        .description = "test app",
        .id = "org.example.app",
        .kind = "app",
        .name = "test app",
        .version = "1.0.0.0",
    };
    project.runtime = std::move(runtime);
    project.version = "1";
    return project;
}

auto builderConfig()
{
    return api::types::v1::BuilderConfig{ .repo = "", .version = 1 };
}

runtime::ContainerBuilder &containerBuilder()
{
    static auto cli = ocppi::cli::crun::Crun::New(std::filesystem::current_path());
    static runtime::ContainerBuilder builder(**cli);
    return builder;
}

class PullDependencyTest : public ::testing::Test
{
protected:
    void SetUp() override { m_repo = std::make_unique<MockRepo>(m_tmpDir.path()); }

    void TearDown() override { m_repo.reset(); }

    TempDir m_tmpDir;
    std::unique_ptr<MockRepo> m_repo;
};

TEST_F(PullDependencyTest, clearDependency_useRemoteFalse_returns_local_ref)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*ref)));

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/x86_64", *m_repo, false);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->toString(), "main:org.example.test/1.0.0.0/x86_64");
}

TEST_F(PullDependencyTest, clearDependency_useRemoteFalse_not_found_returns_error)
{
    LINGLONG_TRACE("clearDependency_useRemoteFalse_not_found_returns_error");

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));

    auto result =
      builder::detail::clearDependency("org.example.missing/1.0.0.0/x86_64", *m_repo, false);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, clearDependency_returns_local_variant)
{
    auto localRef = package::Reference::parse("main:org.example.test/3.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    auto result =
      builder::detail::clearDependency("org.example.test/2.0.0.0/x86_64", *m_repo, true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->version.toString(), "3.0.0.0");
}

TEST_F(PullDependencyTest, clearDependency_exact_version_returns_local_variant)
{
    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/x86_64", *m_repo, true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(result->second);
    EXPECT_FALSE(result->first);
    EXPECT_EQ(result->second->version.toString(), "1.0.0.0");
}

TEST_F(PullDependencyTest, clearDependency_module_missing_returns_remote_variant)
{
    LINGLONG_TRACE("clearDependency_module_missing_returns_remote_variant");

    auto localRef = package::Reference::parse("main:org.example.test/3.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop", _)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef), "custom")));

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/x86_64", *m_repo, true, "develop");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(result->first);
    EXPECT_FALSE(result->second);
    EXPECT_EQ(result->first->repo.name, "custom");
}

TEST_F(PullDependencyTest, useRemoteTrue_prefers_remote_when_newer)
{
    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, "binary", _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullDependency("org.example.test", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "2.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_prefers_local_when_newer)
{
    auto localRef = package::Reference::parse("main:org.example.test/3.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).Times(0);

    auto result =
      builder::detail::pullDependency("org.example.test/2.0.0.0/x86_64", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "3.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_falls_back_to_local_when_remote_pull_fails)
{
    LINGLONG_TRACE("useRemoteTrue_falls_back_to_local_when_remote_pull_fails");

    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, "binary", _)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, pull(_, _, "binary")).WillOnce(Return(LINGLONG_ERR("not found")));

    auto result = builder::detail::pullDependency("org.example.test", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "1.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_equal_version_prefers_local)
{
    auto localRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).Times(0);

    auto result = builder::detail::pullDependency("org.example.test", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, exact_version_local_match_skips_remote_lookup)
{
    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/x86_64", *m_repo, true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->toString(), "main:org.example.test/1.0.0.0/x86_64");
}

TEST_F(PullDependencyTest, useRemoteTrue_only_local_exists)
{
    LINGLONG_TRACE("useRemoteTrue_only_local_exists");

    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("not found")));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).Times(0);

    auto result = builder::detail::pullDependency("org.example.test", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->toString(), "main:org.example.test/1.0.0.0/x86_64");
}

TEST_F(PullDependencyTest, useRemoteTrue_only_remote_exists)
{
    LINGLONG_TRACE("useRemoteTrue_only_remote_exists");

    auto remoteRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/1.0.0.0/x86_64", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, useRemoteTrue_neither_exists_returns_error)
{
    LINGLONG_TRACE("useRemoteTrue_neither_exists_returns_error");

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("not found")));

    auto result =
      builder::detail::pullDependency("org.example.missing/1.0.0.0/x86_64", *m_repo, "binary");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, invalid_fuzzy_ref_returns_error)
{
    auto result = builder::detail::pullDependency("::invalid///ref", *m_repo, "binary");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, pullResolvedRef_cached_module_returns_ok)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullResolvedRef(refWithRepo(std::move(*ref)), *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, pullResolvedRef_not_cached_triggers_pull)
{
    LINGLONG_TRACE("pullResolvedRef_not_cached_triggers_pull");

    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, pull(_, _, _))
      .WillOnce([](service::Task &,
                   const package::ReferenceWithRepo &refRepo,
                   const std::string &) -> utils::error::Result<void> {
          EXPECT_EQ(refRepo.repo.name, "custom");
          return {};
      });

    auto result =
      builder::detail::pullResolvedRef(refWithRepo(std::move(*ref), "custom"), *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, buildStagePullDependency_continues_when_local_develop_pull_fails)
{
    LINGLONG_TRACE("buildStagePullDependency_continues_when_local_develop_pull_fails");

    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(baseRef.has_value());

    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());

    {
        InSequence seq;
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*baseRef));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*baseRef));
    }
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop", _))
      .WillOnce(Return(LINGLONG_ERR("not found")))
      .WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = builder.buildStagePullDependency();
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

} // namespace
