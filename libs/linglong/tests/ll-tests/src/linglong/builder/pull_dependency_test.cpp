// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace linglong;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Return;

namespace {

auto isLocalOption()
{
    return AllOf(Field(&repo::clearReferenceOption::forceRemote, false),
                 Field(&repo::clearReferenceOption::fallbackToRemote, false),
                 Field(&repo::clearReferenceOption::semanticMatching, true));
}

auto isRemoteOption()
{
    return AllOf(Field(&repo::clearReferenceOption::forceRemote, true),
                 Field(&repo::clearReferenceOption::fallbackToRemote, true),
                 Field(&repo::clearReferenceOption::semanticMatching, true));
}

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
                clearReference,
                (const package::FuzzyReference &fuzzy,
                 const repo::clearReferenceOption &opts,
                 const std::string &module,
                 const std::optional<std::string> &repo),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                pull,
                (service::Task & taskContext,
                 const package::ReferenceWithRepo &refRepo,
                 const std::string &module),
                (override, noexcept));

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

auto notFound()
{
    return tl::make_unexpected(utils::error::Error::Err(__FILE__, __LINE__, "", "not found"));
}

auto layerCached()
{
    return package::LayerDir("/fake/layer/dir");
}

class PullDependencyTest : public ::testing::Test
{
protected:
    void SetUp() override { m_repo = std::make_unique<MockRepo>(m_tmpDir.path()); }

    void TearDown() override { m_repo.reset(); }

    TempDir m_tmpDir;
    std::unique_ptr<MockRepo> m_repo;
};

TEST_F(PullDependencyTest, useRemoteFalse_returns_local_ref)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*ref) }));

    auto result =
      builder::detail::pullDependency("org.example.test/1.0.0.0/x86_64", *m_repo, "binary", false);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->toString(), "main:org.example.test/1.0.0.0/x86_64");
}

TEST_F(PullDependencyTest, useRemoteFalse_not_found_returns_error)
{
    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _)).WillOnce(Return(notFound()));

    auto result = builder::detail::pullDependency("org.example.missing/1.0.0.0/x86_64",
                                                  *m_repo,
                                                  "binary",
                                                  false);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, useRemoteTrue_prefers_remote_when_newer)
{
    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*localRef) }));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*remoteRef) }));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/1.0.0.0/x86_64", *m_repo, "binary", true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "2.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_prefers_local_when_newer)
{
    auto localRef = package::Reference::parse("main:org.example.test/3.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*localRef) }));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*remoteRef) }));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/2.0.0.0/x86_64", *m_repo, "binary", true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "3.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_equal_version_prefers_local)
{
    auto localRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    auto remoteRef = package::Reference::parse("main:org.example.test/2.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*localRef) }));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*remoteRef) }));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/2.0.0.0/x86_64", *m_repo, "binary", true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, useRemoteTrue_only_local_exists)
{
    auto localRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*localRef) }));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _)).WillOnce(Return(notFound()));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/1.0.0.0/x86_64", *m_repo, "binary", true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->toString(), "main:org.example.test/1.0.0.0/x86_64");
}

TEST_F(PullDependencyTest, useRemoteTrue_only_remote_exists)
{
    auto remoteRef = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _)).WillOnce(Return(notFound()));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _))
      .WillOnce(Return(utils::error::Result<package::Reference>{ std::move(*remoteRef) }));

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result =
      builder::detail::pullDependency("org.example.test/1.0.0.0/x86_64", *m_repo, "binary", true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, useRemoteTrue_neither_exists_returns_error)
{
    EXPECT_CALL(*m_repo, clearReference(_, isLocalOption(), _, _)).WillOnce(Return(notFound()));
    EXPECT_CALL(*m_repo, clearReference(_, isRemoteOption(), _, _)).WillOnce(Return(notFound()));

    auto result = builder::detail::pullDependency("org.example.missing/1.0.0.0/x86_64",
                                                  *m_repo,
                                                  "binary",
                                                  true);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, invalid_fuzzy_ref_returns_error)
{
    auto result = builder::detail::pullDependency("::invalid///ref", *m_repo, "binary", true);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, pullResolvedRef_cached_module_returns_ok)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullResolvedRef(*ref, *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, pullResolvedRef_not_cached_triggers_pull)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _, _)).WillOnce(Return(notFound()));
    EXPECT_CALL(*m_repo, pull(_, _, _)).WillOnce(Return(utils::error::Result<void>{}));

    auto result = builder::detail::pullResolvedRef(*ref, *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

} // namespace
