// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#define private public
#include "linglong/builder/linglong_builder.h"
#undef private
#include "linglong/builder/source_cache.h"
#include "linglong/builder/source_fetcher.h"
#include "linglong/package/fuzzy_reference.h"
#include "linglong/package/reference.h"
#include "linglong/package_manager/package_task.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <filesystem>
#include <fstream>
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
                (const package::Reference &ref, const std::string &module),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<package::Reference>,
                clearReferenceLocal,
                (const package::FuzzyReference &fuzzy, bool semanticMatching),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<api::types::v1::RepositoryCacheLayersItem>,
                getLayerItem,
                (const package::Reference &ref, std::string module),
                (override, const, noexcept));
    MOCK_METHOD(utils::error::Result<package::ReferenceWithRepo>,
                latestRemoteReference,
                (const package::FuzzyReference &fuzzyRef),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<repo::RemotePackages>,
                matchRemoteByPriority,
                (const package::FuzzyReference &fuzzy,
                 const std::optional<api::types::v1::Repo> &remoteRepo),
                (override, const, noexcept));

    MOCK_METHOD(utils::error::Result<void>,
                pull,
                (service::Task & taskContext,
                 const package::ReferenceWithRepo &refRepo,
                 const std::string &module),
                (override, noexcept));

    MOCK_METHOD(utils::error::Result<void>, mergeModules, (), (override, const, noexcept));

    MOCK_METHOD(std::vector<std::string>,
                getModuleList,
                (const package::Reference &ref),
                (override, const, noexcept));

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

auto remotePackages(const std::vector<package::Reference> &references)
  -> utils::error::Result<repo::RemotePackages>
{
    std::vector<api::types::v1::PackageInfoV2> infos;
    for (const auto &reference : references) {
        api::types::v1::PackageInfoV2 info;
        info.arch = { reference.arch.toString() };
        info.channel = reference.channel;
        info.id = reference.id;
        info.version = reference.version.toString();
        infos.push_back(std::move(info));
    }
    repo::RemotePackages packages;
    packages.addPackages(refWithRepo(references.front(), "custom").repo, std::move(infos));
    return utils::error::Result<repo::RemotePackages>{ std::move(packages) };
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

class SourceTestBuilder : public builder::Builder
{
public:
    using Builder::Builder;
    using Builder::buildStageFetchSource;
};

class PullDependencyTest : public ::testing::Test
{
protected:
    void SetUp() override { m_repo = std::make_unique<MockRepo>(m_tmpDir.path()); }

    void TearDown() override { m_repo.reset(); }

    void expectCachedModule(const package::Reference &reference, const std::string &module)
    {
        EXPECT_CALL(*m_repo, getLayerDir(reference, module))
          .WillRepeatedly([](const package::Reference &,
                             const std::string &) -> utils::error::Result<package::LayerDir> {
              return layerCached();
          });
    }

    void expectFreshBase(const package::Reference &reference)
    {
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(reference));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
        EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
        EXPECT_CALL(*m_repo, getLayerDir(reference, "binary")).WillOnce(Return(layerCached()));
        expectCachedModule(reference, "develop");
        EXPECT_CALL(*m_repo, getLayerItem(reference, _)).Times(0);
        EXPECT_CALL(*m_repo, getModuleList(reference))
          .WillOnce(Return(std::vector<std::string>{ "binary" }));
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
    }

    TempDir m_tmpDir;
    std::unique_ptr<MockRepo> m_repo;
};

TEST_F(PullDependencyTest, MissingLockKeepsNulloptDuringPrepare)
{
    builder::Builder builder(api::types::v1::BuilderProject{},
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.projectLock = api::types::v1::BuilderProjectLock{};
    builder.pendingSources.emplace();

    auto result = builder.buildStagePrepare();
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr("invalid project info"));
    EXPECT_FALSE(builder.projectLock);
    EXPECT_FALSE(builder.pendingSources);
}

TEST_F(PullDependencyTest, SkipFetchLoadsLockBeforeProjectValidation)
{
    builder::Builder builder(api::types::v1::BuilderProject{},
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    const builder::LockedDependency base{ "org.deepin.base/23.0.0",
                                          { { "binary" }, "main:org.deepin.base/23.0.0.0/x86_64" },
                                          "base" };
    ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile,
                                                  builder::completeProjectLock({}, base)));
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipFetchSource = true });

    auto prepared = builder.buildStagePrepare();
    ASSERT_FALSE(prepared);
    EXPECT_THAT(prepared.error().message(), testing::HasSubstr("invalid project info"));
    ASSERT_TRUE(builder.projectLock);
    ASSERT_EQ(builder.projectLock->dependencies.size(), 1);
    EXPECT_EQ(builder.projectLock->dependencies.front().resolved.reference,
              base.resolved.reference);
    EXPECT_FALSE(builder.pendingSources);
}

TEST_F(PullDependencyTest, SkipFetchReusesDependenciesAndPreservesExistingSourceEntries)
{
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    auto project = builderProject();
    project.base = "org.deepin.base/23.0.0";
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "file://" + (m_tmpDir.path() / "unfetched-source").string();
    project.sources = std::vector{ source };
    SourceTestBuilder builder(project,
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    const builder::LockedDependency base{ *project.base,
                                          { { "binary" }, reference->toString() },
                                          "base" };
    auto oldSource = source;
    oldSource.url = "file://" + (m_tmpDir.path() / "old-source").string();
    const auto hash = "sha256:" + std::string(64, 'a');
    builder.projectLock = builder::completeProjectLock({ { { hash }, oldSource } }, base);
    ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
    auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(before);
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });
    auto skipped = builder.buildStageFetchSource();
    ASSERT_TRUE(skipped);
    EXPECT_FALSE(builder.pendingSources);
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).Times(0);
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, getLayerDir(*reference, "binary")).WillOnce(Return(layerCached()));
    EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto fetched = builder.buildStagePullDependency();
    ASSERT_TRUE(fetched) << fetched.error().message();
    EXPECT_EQ(builder.project->base, project.base);
    auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, *before);
    ASSERT_EQ(builder.projectLock->sources.size(), 1);
    EXPECT_EQ(builder.projectLock->sources.front().resolved.hash, hash);
    EXPECT_EQ(builder.projectLock->sources.front().source.url, oldSource.url);
}

TEST_F(PullDependencyTest, SkipFetchUpdatesDependenciesAndKeepsSourceEntriesAfterFailure)
{
    LINGLONG_TRACE("update dependencies after skipping source fetch");
    auto oldReference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto newReference = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    ASSERT_TRUE(oldReference);
    ASSERT_TRUE(newReference);
    auto project = builderProject();
    project.base = "org.deepin.base/24.0.0.0/x86_64";
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "file://" + (m_tmpDir.path() / "unfetched-source").string();
    project.sources = std::vector{ source };
    SourceTestBuilder builder(project,
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    const builder::LockedDependency base{ "org.deepin.base/23.0.0.0/x86_64",
                                          { { "binary" }, oldReference->toString() },
                                          "base" };
    const auto hash = "sha256:" + std::string(64, 'a');
    builder.projectLock = builder::completeProjectLock({ { { hash }, source } }, base);
    ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
    auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(before);
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });
    ASSERT_TRUE(builder.buildStageFetchSource());
    EXPECT_FALSE(builder.pendingSources);

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*newReference));
    expectCachedModule(*newReference, "binary");
    expectCachedModule(*newReference, "develop");
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(LINGLONG_ERR("merge failed")));
    auto failed = builder.buildStagePullDependency();
    ASSERT_FALSE(failed);
    EXPECT_EQ(builder.project->base, project.base);
    EXPECT_EQ(builder.projectLock->dependencies.front().resolved.reference,
              oldReference->toString());
    auto unchanged = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(unchanged);
    EXPECT_EQ(*unchanged, *before);
    testing::Mock::VerifyAndClearExpectations(m_repo.get());

    expectFreshBase(*newReference);
    auto updated = builder.buildStagePullDependency();
    ASSERT_TRUE(updated) << updated.error().message();
    EXPECT_FALSE(builder.pendingSources);
    auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(written);
    ASSERT_EQ(written->dependencies.size(), 1);
    EXPECT_EQ(written->dependencies.front().requested, *project.base);
    EXPECT_EQ(written->dependencies.front().resolved.reference, newReference->toString());
    ASSERT_EQ(written->sources.size(), 1);
    EXPECT_EQ(written->sources.front().resolved.hash, hash);
    EXPECT_EQ(written->sources.front().source.url, source.url);
}

TEST_F(PullDependencyTest, SkipFetchWithoutLockRecordsDependenciesAndLeavesSourcesForNormalBuild)
{
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    auto project = builderProject();
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "file://" + (m_tmpDir.path() / "unfetched-source").string();
    project.sources = std::vector{ source };
    SourceTestBuilder builder(project,
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    const auto sourceDirectory = m_tmpDir.path() / "linglong/sources";
    std::filesystem::create_directories(sourceDirectory);
    const auto manualSource = sourceDirectory / "manual-source";
    std::ofstream(manualSource) << "local changes";
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });
    ASSERT_TRUE(builder.buildStageFetchSource());
    EXPECT_FALSE(builder.projectLock);
    EXPECT_FALSE(builder.pendingSources);
    expectFreshBase(*reference);

    auto fetched = builder.buildStagePullDependency();
    ASSERT_TRUE(fetched) << fetched.error().message();
    auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(written);
    EXPECT_TRUE(written->sources.empty());
    ASSERT_EQ(written->dependencies.size(), 1);
    EXPECT_EQ(written->dependencies.front().resolved.reference, reference->toString());
    EXPECT_TRUE(std::filesystem::exists(manualSource));
    auto next = builder::resolveProjectLockSources(project.sources, *written);
    ASSERT_TRUE(next) << next.error().message();
    EXPECT_TRUE(next->changed);
    ASSERT_EQ(next->requests.size(), 1);
    EXPECT_TRUE(std::holds_alternative<builder::GenerateSourceRequest>(next->requests.front()));
}

TEST_F(PullDependencyTest, FreshSourcesWaitForDependenciesBeforeCreatingLock)
{
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(baseRef) << baseRef.error().message();
    SourceTestBuilder builder(builderProject(),
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                          (m_tmpDir.path() / "cache").string());

    auto fetched = builder.buildStageFetchSource();
    ASSERT_TRUE(fetched) << fetched.error().message();
    EXPECT_FALSE(builder.projectLock);
    ASSERT_TRUE(builder.pendingSources);
    EXPECT_TRUE(builder.pendingSources->empty());
    EXPECT_FALSE(std::filesystem::exists(builder::projectLockPath(builder.projectYamlFile)));

    expectFreshBase(*baseRef);
    auto resolved = builder.buildStagePullDependency();
    ASSERT_TRUE(resolved) << resolved.error().message();
    ASSERT_TRUE(builder.projectLock);
    EXPECT_FALSE(builder.pendingSources);
    auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(written) << written.error().message();
    EXPECT_TRUE(written->sources.empty());
    ASSERT_EQ(written->dependencies.size(), 1);
    EXPECT_EQ(written->dependencies.front().resolved.reference, baseRef->toString());
}

TEST_F(PullDependencyTest, LockedSourceMismatchFailsBeforeResetOrFetch)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.name = "source";
    source.url = "https://example.com/new-source";
    source.digest = "sha256:" + std::string(64, 'a');
    auto project = builderProject();
    project.sources = std::vector<api::types::v1::BuilderProjectSource>{ source };
    SourceTestBuilder builder(project,
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.setBuildOptions(builder::BuilderBuildOptions{ .locked = true });
    auto oldSource = source;
    oldSource.url = "https://example.com/old-source";
    builder.projectLock = api::types::v1::BuilderProjectLock{};
    builder.projectLock->sources = { { { *source.digest }, oldSource } };
    const auto cache = m_tmpDir.path() / "cache";
    utils::EnvironmentVariableGuard cacheEnv("LINGLONG_FETCH_CACHE", cache.string());
    std::filesystem::create_directories(builder.directories.sources());
    const auto manualSource = builder.directories.sources() / "manual-source";
    std::ofstream(manualSource) << "existing sources";

    auto result = builder.buildStageFetchSource();
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr("sources differ from linglong.lock"));
    EXPECT_TRUE(std::filesystem::exists(manualSource));
    EXPECT_FALSE(std::filesystem::exists(cache));
    EXPECT_FALSE(builder.pendingSources);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder.projectLock->sources.front().source.url, oldSource.url);
}

TEST_F(PullDependencyTest, PrintsSourceProgressAndTerminatesRowsOnSuccessAndFailure)
{
    const auto artifact = m_tmpDir.path() / "artifact";
    std::ofstream(artifact) << "source content";
    auto hash = builder::fileHash(artifact);
    ASSERT_TRUE(hash) << hash.error().message();
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.name = "custom-source";
    source.url = "https://example.com/" + std::string(100, 'x') + "/source";
    source.digest = *hash;
    auto project = builderProject();
    project.sources = std::vector<api::types::v1::BuilderProjectSource>{ source };
    SourceTestBuilder builder(project,
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.setBuildOptions(builder::BuilderBuildOptions{ .locked = true, .offline = true });
    builder.projectLock = api::types::v1::BuilderProjectLock{};
    builder.projectLock->sources = { { { *hash }, source } };
    const auto cache = m_tmpDir.path() / "cache";
    utils::EnvironmentVariableGuard cacheEnv("LINGLONG_FETCH_CACHE", cache.string());
    builder::SourceCacheLayout cacheLayout(cache);
    const auto cached = cacheLayout.object("file", *hash);
    std::filesystem::create_directories(cached.parent_path());
    std::filesystem::copy_file(artifact, cached);

    for (const bool corrupt : { false, true }) {
        SCOPED_TRACE(corrupt);
        if (corrupt) {
            std::ofstream(cached, std::ios::trunc) << "corrupt";
        }
        testing::internal::CaptureStdout();
        auto result = builder.buildStageFetchSource();
        const auto output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(result.has_value(), !corrupt);
        EXPECT_THAT(output, testing::HasSubstr("[Processing Sources]"));
        EXPECT_THAT(output, testing::HasSubstr(*source.name));
        EXPECT_THAT(output, testing::Not(testing::HasSubstr("Source 0")));
        EXPECT_THAT(output, testing::HasSubstr("file"));
        EXPECT_THAT(output,
                    testing::HasSubstr("..." + source.url->substr(source.url->size() - 70)));
        EXPECT_THAT(output, testing::HasSubstr("fetching ..."));
        EXPECT_THAT(output, testing::Not(testing::HasSubstr("downloading")));
        EXPECT_THAT(output, testing::HasSubstr(corrupt ? "failed\n" : "complete\n"));
        EXPECT_THAT(output, testing::Not(testing::HasSubstr(corrupt ? "complete\n" : "failed\n")));
    }
}

TEST_F(PullDependencyTest, GenerateAndUpdateCreateLockAfterResolvingDependencies)
{
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(baseRef) << baseRef.error().message();
    for (const auto mode :
         { builder::ProjectLockMode::Generate, builder::ProjectLockMode::Update }) {
        SCOPED_TRACE(static_cast<int>(mode));
        TempDir workingDir;
        builder::Builder builder(builderProject(),
                                 workingDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                              (workingDir.path() / "cache").string());
        if (mode == builder::ProjectLockMode::Update) {
            std::ofstream(builder::projectLockPath(builder.projectYamlFile)) << "invalid old lock";
        }

        expectFreshBase(*baseRef);
        auto result = builder.processProjectLock(mode);
        ASSERT_TRUE(result) << result.error().message();
        ASSERT_TRUE(builder.projectLock);
        EXPECT_FALSE(builder.pendingSources);
        auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(written) << written.error().message();
        EXPECT_TRUE(written->sources.empty());
        ASSERT_EQ(written->dependencies.size(), 1);
        EXPECT_EQ(written->dependencies.front().resolved.reference, baseRef->toString());
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, DependencyFailureKeepsFreshLockAbsent)
{
    LINGLONG_TRACE("fail dependency resolution before creating lock");
    SourceTestBuilder builder(builderProject(),
                              m_tmpDir.path(),
                              *m_repo,
                              containerBuilder(),
                              builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                          (m_tmpDir.path() / "cache").string());
    auto fetched = builder.buildStageFetchSource();
    ASSERT_TRUE(fetched) << fetched.error().message();
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
      .WillOnce(Return(LINGLONG_ERR("dependency is unavailable")));

    auto resolved = builder.buildStagePullDependency();
    ASSERT_FALSE(resolved);
    EXPECT_FALSE(builder.projectLock);
    EXPECT_TRUE(builder.pendingSources);
    EXPECT_FALSE(std::filesystem::exists(builder::projectLockPath(builder.projectYamlFile)));
}

TEST_F(PullDependencyTest, PendingSourcesPreserveExistingLockUntilDependenciesComplete)
{
    LINGLONG_TRACE("fail dependency resolution with pending source changes");
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(baseRef) << baseRef.error().message();
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    const builder::LockedDependency base{ *builder.project->base,
                                          { { "binary" }, baseRef->toString() },
                                          "base" };
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/source";
    const auto oldHash = "sha256:" + std::string(64, 'a');
    const auto newHash = "sha256:" + std::string(64, 'b');
    builder.projectLock = builder::completeProjectLock({ { { oldHash }, source } }, base);
    ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
    builder.pendingSources =
      std::vector<api::types::v1::BuilderSourceLockEntry>{ { { newHash }, source } };
    EXPECT_CALL(*m_repo, getLayerDir(*baseRef, "binary")).WillOnce(Return(layerCached()));
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(LINGLONG_ERR("merge failed")));

    auto resolved = builder.buildStagePullDependency();
    ASSERT_FALSE(resolved);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder.projectLock->sources.front().resolved.hash, oldHash);
    ASSERT_TRUE(builder.pendingSources);
    EXPECT_EQ(builder.pendingSources->front().resolved.hash, newHash);
    auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(written) << written.error().message();
    EXPECT_EQ(written->sources.front().resolved.hash, oldHash);

    EXPECT_CALL(*m_repo, getLayerDir(*baseRef, "binary")).WillOnce(Return(layerCached()));
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).Times(0);
    EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);

    auto retried = builder.buildStagePullDependency();
    ASSERT_TRUE(retried) << retried.error().message();
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder.projectLock->sources.front().resolved.hash, newHash);
    EXPECT_FALSE(builder.pendingSources);
    auto updated = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(updated) << updated.error().message();
    EXPECT_EQ(updated->sources.front().resolved.hash, newHash);
    EXPECT_EQ(updated->dependencies.front().resolved.reference, baseRef->toString());
}

TEST_F(PullDependencyTest, LockedDependencyChangesFailBeforeFetching)
{
    const builder::LockedDependency base{ "org.deepin.base/23.0.0.0/x86_64",
                                          { { "binary" }, "main:org.deepin.base/23.0.0.0/x86_64" },
                                          "base" };
    const builder::LockedDependency runtime{
        "org.deepin.runtime/23.0.0.0/x86_64",
        { { "binary" }, "main:org.deepin.runtime/23.0.0.0/x86_64" },
        "runtime",
    };
    for (const auto &change : { "base", "runtime", "add-runtime", "remove-runtime" }) {
        SCOPED_TRACE(change);
        auto project = builderProject(runtime.requested);
        auto lock = builder::completeProjectLock({}, base, runtime);
        if (std::string(change) == "base") {
            project.base = "org.deepin.base/24.0.0.0/x86_64";
        } else if (std::string(change) == "runtime") {
            project.runtime = "org.deepin.runtime/24.0.0.0/x86_64";
        } else if (std::string(change) == "add-runtime") {
            lock = builder::completeProjectLock({}, base);
        } else {
            project.runtime.reset();
        }
        builder::Builder builder(project,
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        builder.projectLock = lock;
        builder.setBuildOptions(builder::BuilderBuildOptions{ .locked = true });
        ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, lock));
        auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(before);
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
        EXPECT_CALL(*m_repo, getLayerItem(_, _)).Times(0);
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, mergeModules()).Times(0);

        auto result = builder.buildStagePullDependency();
        ASSERT_FALSE(result);
        EXPECT_THAT(result.error().message(), testing::HasSubstr("differs from linglong.lock"));
        EXPECT_EQ(builder.project->base, project.base);
        EXPECT_EQ(builder.project->runtime, project.runtime);
        EXPECT_FALSE(builder.pendingSources);
        auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(after);
        EXPECT_EQ(*after, *before);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, CachedDependencyModulesPrintStatusInBothBuildModes)
{
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    for (const bool locked : { false, true }) {
        SCOPED_TRACE(locked);
        builder::Builder builder(builderProject(),
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        const builder::LockedDependency entry{ *builder.project->base,
                                               { { "binary", "develop" }, reference->toString() },
                                               "base" };
        builder.projectLock = builder::completeProjectLock({}, entry);
        builder.setBuildOptions(
          builder::BuilderBuildOptions{ .skipPullDepend = true, .locked = locked });
        ASSERT_TRUE(
          builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
        auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(before);
        for (const auto &module : { "binary", "develop" }) {
            EXPECT_CALL(*m_repo, getLayerDir(*reference, module)).WillOnce(Return(layerCached()));
        }
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).Times(0);
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);

        testing::internal::CaptureStdout();
        auto result = builder.buildStagePullDependency();
        const auto output = testing::internal::GetCapturedStdout();
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_THAT(output, testing::HasSubstr("binary         complete\n"));
        EXPECT_THAT(output, testing::HasSubstr("develop        complete\n"));
        auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(after);
        EXPECT_EQ(*after, *before);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, SkipPullRejectsMissingLockedModulesWithoutRemoteLookup)
{
    LINGLONG_TRACE("reject unavailable local locked dependency");
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary" }, reference->toString() },
                                           "base" };
    builder.projectLock = builder::completeProjectLock({}, entry);
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    EXPECT_CALL(*m_repo, getLayerDir(*reference, "binary"))
      .WillOnce(Return(LINGLONG_ERR("missing module")));
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    EXPECT_CALL(*m_repo, mergeModules()).Times(0);

    testing::internal::CaptureStdout();
    auto result = builder.buildStagePullDependency();
    const auto output = testing::internal::GetCapturedStdout();
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr("unavailable locally"));
    EXPECT_THAT(output, testing::HasSubstr("binary         failed\n"));
    EXPECT_FALSE(builder.pendingSources);
}

TEST_F(PullDependencyTest, UnchangedRuntimeBaseDeclarationReusesLockedResources)
{
    auto oldBase = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto runtime = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    ASSERT_TRUE(oldBase);
    ASSERT_TRUE(runtime);
    for (const bool locked : { false, true }) {
        SCOPED_TRACE(locked);
        auto project = builderProject("org.deepin.runtime/23.0.0.0/x86_64");
        project.base.reset();
        builder::Builder builder(project,
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        const builder::LockedDependency baseEntry{ "org.deepin.base/23.0.0.0/x86_64",
                                                   { { "binary" }, oldBase->toString() },
                                                   "base" };
        const builder::LockedDependency runtimeEntry{ *project.runtime,
                                                      { { "binary" }, runtime->toString() },
                                                      "runtime" };
        builder.projectLock = builder::completeProjectLock({}, baseEntry, runtimeEntry);
        builder.setBuildOptions(
          builder::BuilderBuildOptions{ .skipPullDepend = true, .locked = locked });
        ASSERT_TRUE(
          builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
        auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(before);
        api::types::v1::RepositoryCacheLayersItem runtimeItem;
        runtimeItem.info.base = baseEntry.requested;
        EXPECT_CALL(*m_repo, getLayerItem(*runtime, "binary"))
          .Times(1)
          .WillRepeatedly([runtimeItem](const package::Reference &, const std::string &)
                            -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
              return runtimeItem;
          });
        EXPECT_CALL(*m_repo, getLayerDir(*runtime, "binary")).WillOnce(Return(layerCached()));
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).Times(0);
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
        EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
        EXPECT_CALL(*m_repo, getLayerDir(*oldBase, "binary")).WillOnce(Return(layerCached()));
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

        auto result = builder.buildStagePullDependency();
        ASSERT_TRUE(result) << result.error().message();
        auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(after);
        EXPECT_EQ(builder.project->base, project.base);
        EXPECT_EQ(*after, *before);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, RemovingExplicitBaseFollowsRuntimeDeclaration)
{
    LINGLONG_TRACE("derive base declaration after removing explicit base");

    auto oldBase = package::Reference::parse("main:org.test.Base/1.0.1/x86_64");
    auto newBase = package::Reference::parse("main:org.test.Base/1.0.2/x86_64");
    auto runtime = package::Reference::parse("main:org.test.Runtime/2.0.0/x86_64");
    ASSERT_TRUE(oldBase);
    ASSERT_TRUE(newBase);
    ASSERT_TRUE(runtime);

    for (const bool differentReference : { false, true }) {
        SCOPED_TRACE(differentReference);
        const auto &expectedBase = differentReference ? *newBase : *oldBase;
        for (const bool locked : { false, true }) {
            SCOPED_TRACE(locked);
            auto project = builderProject("org.test.Runtime/2.0");
            project.base = "org.test.Base/1.0.1";
            const builder::LockedDependency baseEntry{ *project.base,
                                                       { { "binary" }, oldBase->toString() },
                                                       "base" };
            const builder::LockedDependency runtimeEntry{ *project.runtime,
                                                          { { "binary" }, runtime->toString() },
                                                          "runtime" };
            const auto originalLock = builder::completeProjectLock({}, baseEntry, runtimeEntry);
            auto previousState = builder::projectLockDependencyState(originalLock);
            ASSERT_TRUE(previousState);

            project.base.reset();
            builder::Builder builder(project,
                                     m_tmpDir.path(),
                                     *m_repo,
                                     containerBuilder(),
                                     builderConfig());
            builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
            builder.projectLock = originalLock;
            builder.setBuildOptions(
              builder::BuilderBuildOptions{ .skipPullDepend = true, .locked = locked });
            ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, originalLock));
            auto before = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
            ASSERT_TRUE(before);

            api::types::v1::RepositoryCacheLayersItem runtimeItem;
            runtimeItem.info.base = "org.test.Base/1.0";
            EXPECT_CALL(*m_repo, getLayerDir(*runtime, "binary")).WillOnce(Return(layerCached()));
            EXPECT_CALL(*m_repo, getLayerItem(*runtime, "binary")).WillOnce(Return(runtimeItem));
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
            EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
            EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
            EXPECT_CALL(*m_repo, getModuleList(*runtime)).Times(0);
            if (locked) {
                EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).Times(0);
                EXPECT_CALL(*m_repo, getLayerDir(*oldBase, _)).Times(0);
                EXPECT_CALL(*m_repo, getLayerDir(*newBase, _)).Times(0);
                EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
                EXPECT_CALL(*m_repo, mergeModules()).Times(0);
            } else {
                EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
                  .WillOnce([&expectedBase](const package::FuzzyReference &requested, bool) {
                      EXPECT_EQ(requested.id, "org.test.Base");
                      EXPECT_EQ(requested.version, "1.0");
                      return expectedBase;
                  });
                EXPECT_CALL(*m_repo, getLayerDir(expectedBase, "binary"))
                  .WillOnce(Return(layerCached()));
                EXPECT_CALL(*m_repo, getLayerDir(expectedBase, "develop"))
                  .WillOnce(Return(LINGLONG_ERR("develop module missing")));
                EXPECT_CALL(*m_repo, getModuleList(expectedBase))
                  .WillOnce(Return(std::vector<std::string>{ "binary" }));
                EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
            }

            auto result = builder.buildStagePullDependency();
            auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
            ASSERT_TRUE(after);
            EXPECT_FALSE(builder.project->base);
            EXPECT_EQ(builder.project->runtime, project.runtime);
            if (locked) {
                ASSERT_FALSE(result);
                EXPECT_THAT(result.error().message(), testing::HasSubstr("base differs"));
                EXPECT_EQ(*after, *before);
                testing::Mock::VerifyAndClearExpectations(m_repo.get());
                continue;
            }

            ASSERT_TRUE(result) << result.error().message();
            EXPECT_NE(*after, *before);
            auto written =
              builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
            ASSERT_TRUE(written);
            auto writtenBase = builder::lockedDependency(*written, "base");
            auto writtenRuntime = builder::lockedDependency(*written, "runtime");
            ASSERT_TRUE(writtenBase);
            ASSERT_TRUE(writtenRuntime);
            EXPECT_EQ(writtenBase->requested, runtimeItem.info.base);
            EXPECT_EQ(writtenBase->resolved.reference, expectedBase.toString());
            EXPECT_EQ(writtenRuntime->requested, runtimeEntry.requested);
            EXPECT_EQ(writtenRuntime->resolved.reference, runtimeEntry.resolved.reference);
            auto state = builder::projectLockDependencyState(*written);
            ASSERT_TRUE(state);
            EXPECT_EQ(*state == *previousState, !differentReference);

            TempDir workingDir;
            const auto root = workingDir.path() / "repo";
            std::filesystem::create_directories(root);
            auto repo = repo::OSTreeRepo::create(root, m_repo->getConfig());
            ASSERT_TRUE(repo) << repo.error().message();
            builder::Builder committed(project,
                                       workingDir.path(),
                                       **repo,
                                       containerBuilder(),
                                       builderConfig());
            committed.projectLock = builder.projectLock;
            auto reference = package::Reference::fromBuilderProject(project);
            ASSERT_TRUE(reference);
            committed.projectRef = *reference;
            committed.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
            std::ofstream(committed.projectYamlFile) << "version: 1\n";
            committed.packageModules = { "binary" };
            const auto output = committed.directories.module("binary");
            std::filesystem::create_directories(output / "files");
            std::ofstream(output / "files/payload") << "content";

            auto committedResult = committed.commitToLocalRepo();
            ASSERT_TRUE(committedResult) << committedResult.error().message();
            auto layer = (*repo)->getLayerDir(*reference, "binary");
            ASSERT_TRUE(layer);
            auto info = layer->info();
            ASSERT_TRUE(info);
            EXPECT_EQ(info->base, runtimeItem.info.base);
            EXPECT_EQ(info->runtime, project.runtime);
            testing::Mock::VerifyAndClearExpectations(m_repo.get());
        }
    }
}

TEST_F(PullDependencyTest, FreshRuntimeDerivesBaseWhenYamlOmitsBase)
{
    auto oldBase = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto oldRuntime = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    auto newBase = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    auto newRuntime = package::Reference::parse("main:org.deepin.runtime/24.0.0.0/x86_64");
    ASSERT_TRUE(oldBase);
    ASSERT_TRUE(oldRuntime);
    ASSERT_TRUE(newBase);
    ASSERT_TRUE(newRuntime);
    for (const int scenario : { 0, 1, 2 }) {
        SCOPED_TRACE(scenario);
        const bool existingLock = scenario != 0;
        const bool reuseBase = scenario == 2;
        const auto &expectedBase = reuseBase ? *oldBase : *newBase;
        auto project = builderProject("org.deepin.runtime/24.0.0.0/x86_64");
        project.base.reset();
        builder::Builder builder(project,
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        if (existingLock) {
            const builder::LockedDependency baseEntry{ "org.deepin.base/23.0.0.0/x86_64",
                                                       { { "binary" }, oldBase->toString() },
                                                       "base" };
            const builder::LockedDependency runtimeEntry{ "org.deepin.runtime/23.0.0.0/x86_64",
                                                          { { "binary" }, oldRuntime->toString() },
                                                          "runtime" };
            builder.projectLock = builder::completeProjectLock({}, baseEntry, runtimeEntry);
        } else {
            builder.pendingSources.emplace();
        }

        if (reuseBase) {
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*newRuntime));
        } else {
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
              .WillOnce(Return(*newRuntime))
              .WillOnce(Return(expectedBase));
        }
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
        EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, getLayerDir(_, _))
          .WillRepeatedly([](const package::Reference &,
                             const std::string &) -> utils::error::Result<package::LayerDir> {
              return layerCached();
          });
        api::types::v1::RepositoryCacheLayersItem runtimeItem;
        runtimeItem.info.base =
          reuseBase ? "org.deepin.base/23.0.0.0/x86_64" : "org.deepin.base/24.0.0.0/x86_64";
        EXPECT_CALL(*m_repo, getLayerItem(*newRuntime, "binary"))
          .Times(1)
          .WillRepeatedly([runtimeItem](const package::Reference &, const std::string &)
                            -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
              return runtimeItem;
          });
        EXPECT_CALL(*m_repo, getLayerItem(expectedBase, _)).Times(0);
        if (reuseBase) {
            EXPECT_CALL(*m_repo, getModuleList(expectedBase)).Times(0);
        } else {
            EXPECT_CALL(*m_repo, getModuleList(expectedBase))
              .WillOnce(Return(std::vector<std::string>{ "binary" }));
        }
        EXPECT_CALL(*m_repo, getModuleList(*newRuntime))
          .WillOnce(Return(std::vector<std::string>{ "binary" }));
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

        auto result = builder.buildStagePullDependency();
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_EQ(builder.project->base, project.base);
        EXPECT_EQ(builder.project->runtime, project.runtime);
        EXPECT_FALSE(builder.pendingSources);
        auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(written);
        ASSERT_EQ(written->dependencies.size(), 2);
        EXPECT_EQ(written->dependencies[0].requested, runtimeItem.info.base);
        EXPECT_EQ(written->dependencies[0].resolved.reference, expectedBase.toString());
        EXPECT_EQ(written->dependencies[1].requested, *project.runtime);
        EXPECT_EQ(written->dependencies[1].resolved.reference, newRuntime->toString());
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, CommittedMetadataPreservesRuntimeDependencyDeclarations)
{
    for (const int mode : { 0, 1, 2 }) {
        SCOPED_TRACE(mode);
        const bool withRuntime = mode != 0;
        TempDir workingDir;
        const auto root = workingDir.path() / "repo";
        std::filesystem::create_directories(root);
        auto repo = repo::OSTreeRepo::create(root, m_repo->getConfig());
        ASSERT_TRUE(repo) << repo.error().message();
        auto project = builderProject(
          withRuntime ? std::make_optional(std::string("org.test.Runtime/2.0.0")) : std::nullopt);
        if (mode == 2) {
            project.base.reset();
        } else {
            project.base = "org.test.Base/1.0.0";
        }
        builder::Builder builder(project,
                                 workingDir.path(),
                                 **repo,
                                 containerBuilder(),
                                 builderConfig());
        const builder::LockedDependency base{ "org.test.Base/1.0.0",
                                              { { "binary" }, "main:org.test.Base/1.0.0.0/x86_64" },
                                              "base" };
        const builder::LockedDependency runtime{ "org.test.Runtime/2.0.0",
                                                 { { "binary" },
                                                   "main:org.test.Runtime/2.0.0.0/x86_64" },
                                                 "runtime" };
        builder.projectLock =
          builder::completeProjectLock({},
                                       base,
                                       withRuntime ? std::make_optional(runtime) : std::nullopt);
        auto reference = package::Reference::fromBuilderProject(project);
        ASSERT_TRUE(reference);
        builder.projectRef = *reference;
        builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
        std::ofstream(builder.projectYamlFile) << "version: 1\n";
        builder.packageModules = { "binary", "develop" };
        for (const auto &module : builder.packageModules) {
            const auto output = builder.directories.module(module);
            std::filesystem::create_directories(output / "files");
            std::ofstream(output / "files/payload") << module;
        }

        auto committed = builder.commitToLocalRepo();
        ASSERT_TRUE(committed) << committed.error().message();
        for (const auto &module : builder.packageModules) {
            const auto layer = (*repo)->getLayerDir(*reference, module);
            ASSERT_TRUE(layer);
            const auto info = layer->info();
            ASSERT_TRUE(info);
            EXPECT_EQ(info->base, "org.test.Base/1.0.0");
            EXPECT_EQ(info->runtime, project.runtime);
        }
        EXPECT_EQ(builder.project->base, project.base);
        EXPECT_EQ(builder.project->runtime, project.runtime);
    }
}

TEST_F(PullDependencyTest, PreservesLegacyRuntimeModuleInLock)
{
    const auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    EXPECT_CALL(*m_repo, getModuleList(*reference))
      .WillOnce(Return(std::vector<std::string>{ "runtime", "develop" }));

    const auto entry = builder.recordProjectDependency(
      builder::ResolveDependencyRequest{ *builder.project->base, "base" },
      *reference);
    ASSERT_TRUE(entry) << entry.error().message();
    EXPECT_EQ(entry->resolved.modules, (std::vector<std::string>{ "develop", "runtime" }));
    const auto manifest = m_tmpDir.path() / "linglong.yaml";
    ASSERT_TRUE(
      builder::writeCompleteProjectLock(manifest, builder::completeProjectLock({}, *entry)));
    const auto loaded = builder::readProjectLock(builder::projectLockPath(manifest));
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->dependencies.front().resolved.modules, entry->resolved.modules);
}

TEST_F(PullDependencyTest, LockedModulesShareExactRemoteLookup)
{
    LINGLONG_TRACE("fetch locked modules with one remote lookup");
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary", "develop" }, reference->toString() },
                                           "base" };
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
      .WillOnce([&](const package::FuzzyReference &fuzzy,
                    const auto &) -> utils::error::Result<repo::RemotePackages> {
          EXPECT_EQ(fuzzy.toString(), reference->toString());
          return remotePackages({ *newer, *reference });
      });
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, getLayerItem(_, _)).Times(0);
    for (const std::string module : { "binary", "develop" }) {
        EXPECT_CALL(*m_repo, getLayerDir(*reference, module))
          .WillOnce(Return(LINGLONG_ERR("not installed")))
          .WillOnce(Return(LINGLONG_ERR("not installed")))
          .WillOnce(Return(layerCached()));
        EXPECT_CALL(*m_repo, pull(_, _, module))
          .WillOnce([&](service::Task &,
                        const package::ReferenceWithRepo &remote,
                        const auto &) -> utils::error::Result<void> {
              EXPECT_EQ(remote.reference, *reference);
              EXPECT_EQ(remote.repo.name, "custom");
              return LINGLONG_OK;
          });
    }
    testing::internal::CaptureStdout();
    auto result = builder.fetchProjectDependency(builder::LockedDependencyRequest{ entry });
    const auto output = testing::internal::GetCapturedStdout();
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(*result, *reference);
    EXPECT_THAT(output, testing::HasSubstr("binary         complete\n"));
    EXPECT_THAT(output, testing::HasSubstr("develop        complete\n"));
}

TEST_F(PullDependencyTest, LockedDependencyRejectsOtherRemoteVersions)
{
    LINGLONG_TRACE("reject replacement versions for locked dependency");
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary" }, reference->toString() },
                                           "base" };
    EXPECT_CALL(*m_repo, getLayerDir(*reference, "binary"))
      .WillOnce(Return(LINGLONG_ERR("not installed")));
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
      .WillOnce([&](const package::FuzzyReference &,
                    const auto &) -> utils::error::Result<repo::RemotePackages> {
          return remotePackages({ *newer });
      });
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    auto result = builder.fetchProjectDependency(builder::LockedDependencyRequest{ entry });
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr("unavailable remotely"));
}

TEST_F(PullDependencyTest, MissingDevelopIsRequiredByLockAndOptionalForResolution)
{
    LINGLONG_TRACE("check required and optional develop modules without remote access");
    auto reference = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(reference);
    for (const bool locked : { false, true }) {
        SCOPED_TRACE(locked);
        builder::Builder builder(builderProject(),
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        builder::DependencyFetchRequest request =
          builder::ResolveDependencyRequest{ *builder.project->base, "base" };
        if (locked) {
            request = builder::LockedDependencyRequest{ { *builder.project->base,
                                                          { { "binary", "develop" },
                                                            reference->toString() },
                                                          "base" } };
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
        } else {
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*reference));
        }
        expectCachedModule(*reference, "binary");
        EXPECT_CALL(*m_repo, getLayerDir(*reference, "develop"))
          .WillOnce(Return(LINGLONG_ERR("not installed")));
        EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
        testing::internal::CaptureStdout();
        auto result = builder.fetchProjectDependency(request);
        const auto output = testing::internal::GetCapturedStdout();
        EXPECT_THAT(output, testing::HasSubstr("binary         complete\n"));
        if (locked) {
            ASSERT_FALSE(result);
            EXPECT_THAT(output, testing::HasSubstr("develop        failed\n"));
        } else {
            ASSERT_TRUE(result) << result.error().message();
            EXPECT_EQ(*result, *reference);
            EXPECT_THAT(output, testing::HasSubstr("develop        missing\n"));
        }
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, ResolvedBinaryFallbackFetchesDevelopForFinalReference)
{
    LINGLONG_TRACE("fetch develop for the local fallback version");
    auto local = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    ASSERT_TRUE(local);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*local));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(refWithRepo(*newer)));
    EXPECT_CALL(*m_repo, getLayerDir(*newer, "binary"))
      .WillOnce(Return(LINGLONG_ERR("not installed")))
      .WillOnce(Return(LINGLONG_ERR("not installed")));
    EXPECT_CALL(*m_repo, pull(_, _, "binary"))
      .WillOnce([&](service::Task &,
                    const package::ReferenceWithRepo &remote,
                    const auto &) -> utils::error::Result<void> {
          EXPECT_EQ(remote.reference, *newer);
          return LINGLONG_ERR("download failed");
      });
    expectCachedModule(*local, "binary");
    EXPECT_CALL(*m_repo, getLayerDir(*local, "develop"))
      .WillOnce(Return(LINGLONG_ERR("not installed")))
      .WillOnce(Return(LINGLONG_ERR("not installed")))
      .WillOnce(Return(layerCached()));
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
      .WillOnce([&](const package::FuzzyReference &fuzzy,
                    const auto &) -> utils::error::Result<repo::RemotePackages> {
          EXPECT_EQ(fuzzy.toString(), local->toString());
          return remotePackages({ *newer, *local });
      });
    EXPECT_CALL(*m_repo, pull(_, _, "develop"))
      .WillOnce([&](service::Task &,
                    const package::ReferenceWithRepo &remote,
                    const auto &) -> utils::error::Result<void> {
          EXPECT_EQ(remote.reference, *local);
          return LINGLONG_OK;
      });
    EXPECT_CALL(*m_repo, getLayerDir(*newer, "develop")).Times(0);
    EXPECT_CALL(*m_repo, getLayerItem(_, _)).Times(0);
    testing::internal::CaptureStdout();
    auto result = builder.fetchProjectDependency(
      builder::ResolveDependencyRequest{ "org.deepin.base", "base" });
    const auto output = testing::internal::GetCapturedStdout();
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(*result, *local);
    EXPECT_THAT(output, testing::HasSubstr("binary         complete\n"));
    EXPECT_THAT(output, testing::HasSubstr("develop        complete\n"));
}

TEST_F(PullDependencyTest, CrossBuildGeneratesAndUpdatesLock)
{
    auto base = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(base);
    for (const auto mode :
         { builder::ProjectLockMode::Generate, builder::ProjectLockMode::Update }) {
        SCOPED_TRACE(static_cast<int>(mode));
        TempDir workingDir;
        auto project = builderProject();
        project.package.architecture = "arm64";
        project.base = "org.deepin.base/23.0.0.0";
        builder::Builder builder(project,
                                 workingDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (workingDir.path() / "linglong.arm64.yaml").string();
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                              (workingDir.path() / "cache").string());
        if (mode == builder::ProjectLockMode::Update) {
            const builder::LockedDependency oldBase{ *project.base,
                                                     { { "binary" },
                                                       "main:org.deepin.base/23.0.0.0/arm64" },
                                                     "base" };
            auto oldLock = builder::completeProjectLock({}, oldBase);
            oldLock.buildArchitecture =
              package::Architecture::currentCPUArchitecture().toString() == "arm64" ? "x86_64"
                                                                                    : "arm64";
            ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, oldLock));
        }

        expectFreshBase(*base);
        auto result = builder.processProjectLock(mode);
        ASSERT_TRUE(result) << result.error().message();
        auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(written);
        EXPECT_EQ(written->buildArchitecture,
                  package::Architecture::currentCPUArchitecture().toString());
        auto baseEntry = builder::lockedDependency(*written, "base");
        ASSERT_TRUE(baseEntry);
        EXPECT_EQ(baseEntry->requested, *project.base);
        EXPECT_EQ(baseEntry->resolved.reference, base->toString());
        EXPECT_EQ(builder.project->package.architecture, "arm64");
        EXPECT_EQ(builder.project->base, project.base);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, PrepareRejectsDifferentBuildArchitecture)
{
    const auto current = package::Architecture::currentCPUArchitecture().toString();
    const std::string other = current == "arm64" ? "x86_64" : "arm64";
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

    for (const bool explicitTarget : { false, true }) {
        SCOPED_TRACE(explicitTarget);
        for (const int mode : { 0, 1, 2, 3 }) {
            SCOPED_TRACE(mode);
            TempDir workingDir;
            auto project = builderProject();
            if (!explicitTarget) {
                project.package.architecture.reset();
            }

            builder::Builder builder(project,
                                     workingDir.path(),
                                     *m_repo,
                                     containerBuilder(),
                                     builderConfig());
            builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
            builder.setBuildOptions(builder::BuilderBuildOptions{ .skipFetchSource = mode == 2,
                                                                  .locked = mode == 1,
                                                                  .offline = mode == 3 });
            const builder::LockedDependency base{ *project.base,
                                                  { { "binary" },
                                                    "main:org.deepin.base/23.0.0.0/x86_64" },
                                                  "base" };
            auto lock = builder::completeProjectLock({}, base);
            lock.buildArchitecture = other;
            ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, lock));
            const auto path = builder::projectLockPath(builder.projectYamlFile);
            auto before = utils::readFile(path);
            ASSERT_TRUE(before);

            auto result = builder.buildStagePrepare();
            ASSERT_FALSE(result);
            EXPECT_THAT(result.error().message(),
                        testing::HasSubstr("build architecture mismatch"));
            EXPECT_THAT(result.error().message(), testing::HasSubstr("ll-builder update-lockfile"));
            EXPECT_FALSE(builder.projectLock);
            EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong/sources"));
            auto after = utils::readFile(path);
            ASSERT_TRUE(after);
            EXPECT_EQ(*after, *before);
        }
    }
}

TEST_F(PullDependencyTest, FailedArchitectureUpdatePreservesOldLock)
{
    auto project = builderProject();
    project.base = "org.deepin.base/23.0.0.0";
    builder::Builder builder(project,
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                          (m_tmpDir.path() / "cache").string());
    const auto current = package::Architecture::currentCPUArchitecture();
    const std::string other = current.toString() == "arm64" ? "x86_64" : "arm64";
    const builder::LockedDependency base{ *project.base,
                                          { { "binary" },
                                            "main:org.deepin.base/23.0.0.0/" + other },
                                          "base" };
    auto lock = builder::completeProjectLock({}, base);
    lock.buildArchitecture = other;
    ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, lock));
    const auto path = builder::projectLockPath(builder.projectYamlFile);
    auto before = utils::readFile(path);
    ASSERT_TRUE(before);
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
      .WillOnce([&](const package::FuzzyReference &fuzzy,
                    bool) -> utils::error::Result<package::Reference> {
          LINGLONG_TRACE("look up a dependency for the new build architecture");
          EXPECT_EQ(fuzzy.arch, current);
          return LINGLONG_ERR("not installed for current architecture");
      });
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);

    auto result = builder.processProjectLock(builder::ProjectLockMode::Update);
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr("failed to get local ref"));
    auto after = utils::readFile(path);
    ASSERT_TRUE(after);
    EXPECT_EQ(*after, *before);
}

TEST_F(PullDependencyTest, ResolvePreservesExplicitDependencyArchitecture)
{
    const auto current = package::Architecture::currentCPUArchitecture().toString();
    const std::string other = current == "arm64" ? "x86_64" : "arm64";
    const auto requested = "org.deepin.base/23.0.0.0/" + other;
    auto reference = package::Reference::parse("main:" + requested);
    ASSERT_TRUE(reference);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
      .WillOnce([&](const package::FuzzyReference &fuzzy, bool) {
          EXPECT_EQ(fuzzy.arch, reference->arch);
          return *reference;
      });
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    expectCachedModule(*reference, "binary");
    expectCachedModule(*reference, "develop");

    auto result =
      builder.fetchProjectDependency(builder::ResolveDependencyRequest{ requested, "base" });
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(*result, *reference);
}

TEST_F(PullDependencyTest, CrossBuildPreservesDependencyArchitectures)
{
    LINGLONG_TRACE("resolve and reuse host dependencies for a cross build");

    auto base = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto runtime = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    ASSERT_TRUE(base);
    ASSERT_TRUE(runtime);

    for (const bool withRuntime : { false, true }) {
        SCOPED_TRACE(withRuntime);
        for (const int scenario : { 0, 1, 2 }) {
            SCOPED_TRACE(scenario);
            const bool existingLock = scenario != 0;
            auto project = builderProject();
            project.package.architecture = "arm64";
            project.base = "org.deepin.base/23.0.0";
            if (withRuntime) {
                project.runtime = "org.deepin.runtime/23.0.0";
                project.base.reset();
            }

            builder::Builder builder(project,
                                     m_tmpDir.path(),
                                     *m_repo,
                                     containerBuilder(),
                                     builderConfig());
            builder.projectYamlFile = (m_tmpDir.path()
                                       / ("cross-" + std::to_string(withRuntime) + "-"
                                          + std::to_string(scenario) + ".yaml"))
                                        .string();
            builder.setBuildOptions(builder::BuilderBuildOptions{ .locked = scenario == 2 });
            const builder::LockedDependency baseEntry{ "org.deepin.base/23.0.0",
                                                       { { "binary", "develop" },
                                                         base->toString() },
                                                       "base" };
            std::optional<builder::LockedDependency> runtimeEntry;
            if (withRuntime) {
                runtimeEntry =
                  builder::LockedDependency{ *project.runtime,
                                             { { "binary", "develop" }, runtime->toString() },
                                             "runtime" };
            }
            std::optional<std::string> before;
            if (existingLock) {
                builder.projectLock = builder::completeProjectLock({}, baseEntry, runtimeEntry);
                ASSERT_TRUE(
                  builder::writeCompleteProjectLock(builder.projectYamlFile, *builder.projectLock));
                auto contents = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
                ASSERT_TRUE(contents);
                before = *contents;
                EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
                EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
                EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
            }

            auto expectDependency = [&](const package::Reference &reference) {
                expectCachedModule(reference, "binary");
                expectCachedModule(reference, "develop");
                if (existingLock) {
                    return;
                }

                EXPECT_CALL(
                  *m_repo,
                  clearReferenceLocal(testing::Field(&package::FuzzyReference::id, reference.id),
                                      true))
                  .WillOnce([&reference](const package::FuzzyReference &fuzzy,
                                         bool) -> utils::error::Result<package::Reference> {
                      LINGLONG_TRACE("look up a cross build dependency locally");
                      EXPECT_EQ(fuzzy.id, reference.id);
                      EXPECT_EQ(fuzzy.arch, package::Architecture::currentCPUArchitecture());
                      return LINGLONG_ERR("not installed");
                  });
                EXPECT_CALL(
                  *m_repo,
                  latestRemoteReference(testing::Field(&package::FuzzyReference::id, reference.id)))
                  .WillOnce([&reference](const package::FuzzyReference &fuzzy) {
                      EXPECT_EQ(fuzzy.id, reference.id);
                      EXPECT_EQ(fuzzy.arch, package::Architecture::currentCPUArchitecture());
                      return refWithRepo(reference);
                  });
                EXPECT_CALL(*m_repo, getModuleList(reference))
                  .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));
            };
            if (withRuntime) {
                expectDependency(*runtime);
                api::types::v1::RepositoryCacheLayersItem runtimeItem;
                runtimeItem.info.base = baseEntry.requested;
                EXPECT_CALL(*m_repo, getLayerItem(*runtime, "binary"))
                  .WillOnce(Return(runtimeItem));
            }
            expectDependency(*base);
            EXPECT_CALL(*m_repo, getLayerItem(*base, _)).Times(0);
            EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
            EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
            EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

            auto result = builder.buildStagePullDependency();
            ASSERT_TRUE(result) << result.error().message();
            auto written =
              builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
            ASSERT_TRUE(written);
            auto writtenBase = builder::lockedDependency(*written, "base");
            ASSERT_TRUE(writtenBase);
            EXPECT_EQ(writtenBase->resolved.reference, base->toString());
            if (withRuntime) {
                auto writtenRuntime = builder::lockedDependency(*written, "runtime");
                ASSERT_TRUE(writtenRuntime);
                EXPECT_EQ(writtenRuntime->resolved.reference, runtime->toString());
            }
            EXPECT_EQ(builder.project->package.architecture, "arm64");
            EXPECT_EQ(builder.project->base, project.base);
            EXPECT_EQ(builder.project->runtime, project.runtime);
            if (before) {
                auto after = utils::readFile(builder::projectLockPath(builder.projectYamlFile));
                ASSERT_TRUE(after);
                EXPECT_EQ(*after, *before);
            }
            testing::Mock::VerifyAndClearExpectations(m_repo.get());
        }
    }
}

TEST_F(PullDependencyTest, CrossBuildKeepsLocalFallbackAfterDownloadFailure)
{
    LINGLONG_TRACE("use a local host dependency after a cross build download fails");

    auto local = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    auto remote = package::Reference::parse("main:org.deepin.base/24.0.0.0/x86_64");
    ASSERT_TRUE(local);
    ASSERT_TRUE(remote);
    auto project = builderProject();
    project.package.architecture = "arm64";
    project.base = "org.deepin.base";
    builder::Builder builder(project,
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "cross-fallback.yaml").string();

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*local));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(refWithRepo(*remote)));
    EXPECT_CALL(*m_repo, getLayerDir(*remote, "binary"))
      .WillOnce(Return(LINGLONG_ERR("not installed")))
      .WillOnce(Return(LINGLONG_ERR("not installed")));
    EXPECT_CALL(*m_repo, pull(_, _, "binary")).WillOnce(Return(LINGLONG_ERR("download failed")));
    expectCachedModule(*local, "binary");
    expectCachedModule(*local, "develop");
    EXPECT_CALL(*m_repo, getLayerDir(*remote, "develop")).Times(0);
    EXPECT_CALL(*m_repo, getLayerItem(_, _)).Times(0);
    EXPECT_CALL(*m_repo, getModuleList(*local))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result) << result.error().message();
    auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
    ASSERT_TRUE(written);
    auto baseEntry = builder::lockedDependency(*written, "base");
    ASSERT_TRUE(baseEntry);
    EXPECT_EQ(baseEntry->resolved.reference, local->toString());
    EXPECT_EQ(builder.project->package.architecture, "arm64");
}

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
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop")).WillOnce(Return(LINGLONG_ERR("not found")));
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

    EXPECT_CALL(*m_repo, getLayerDir(_, "binary")).WillOnce(Return(layerCached()));

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

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

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

    EXPECT_CALL(*m_repo, getLayerDir(_, "binary")).WillOnce(Return(LINGLONG_ERR("not found")));
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

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

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

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

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

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).WillOnce(Return(layerCached()));

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

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullResolvedRef(refWithRepo(std::move(*ref)), *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, pullResolvedRef_not_cached_triggers_pull)
{
    LINGLONG_TRACE("pullResolvedRef_not_cached_triggers_pull");

    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/x86_64");
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).WillOnce(Return(LINGLONG_ERR("not found")));
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
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipFetchSource = true });
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();

    expectFreshBase(*baseRef);
    EXPECT_CALL(*m_repo, getLayerDir(*baseRef, "develop"))
      .WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);

    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.project->base);
    EXPECT_EQ(builder.project->base, builderProject().base);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_TRUE(builder.projectLock->sources.empty());
    ASSERT_EQ(builder.projectLock->dependencies.size(), 1);
    EXPECT_EQ(builder.projectLock->dependencies.front().resolved.reference, baseRef->toString());
    EXPECT_FALSE(builder.pendingSources);
    EXPECT_TRUE(std::filesystem::exists(builder::projectLockPath(builder.projectYamlFile)));
}

TEST_F(PullDependencyTest, buildStagePullDependency_resolves_base_from_runtime)
{
    LINGLONG_TRACE("buildStagePullDependency_resolves_base_from_runtime");

    auto runtimeRef = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.0/x86_64");
    ASSERT_TRUE(runtimeRef.has_value());
    ASSERT_TRUE(baseRef.has_value());

    auto project = builderProject("org.deepin.runtime/23.0.0.0/x86_64");
    project.base.reset();
    builder::Builder builder(std::move(project),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });

    {
        InSequence seq;
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*runtimeRef));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*baseRef));
    }
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.base = "org.deepin.base/23.0.0.0/x86_64";
    EXPECT_CALL(*m_repo, getLayerItem(*runtimeRef, "binary"))
      .Times(1)
      .WillRepeatedly([runtimeItem](const package::Reference &, const std::string &)
                        -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
          return runtimeItem;
      });
    EXPECT_CALL(*m_repo, getLayerDir(_, "binary"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
    EXPECT_CALL(*m_repo, getLayerItem(*baseRef, _)).Times(0);
    EXPECT_CALL(*m_repo, getModuleList(*baseRef))
      .WillOnce(Return(std::vector<std::string>{ "binary" }));
    EXPECT_CALL(*m_repo, getModuleList(*runtimeRef))
      .WillOnce(Return(std::vector<std::string>{ "binary" }));
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(builder.project->base);
    ASSERT_TRUE(builder.project->runtime);
    EXPECT_EQ(*builder.project->runtime, "org.deepin.runtime/23.0.0.0/x86_64");
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "base")->resolved.reference,
              baseRef->toString());
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "runtime")->resolved.reference,
              runtimeRef->toString());
}

TEST_F(PullDependencyTest, buildStagePullDependency_rejects_incompatible_base_and_runtime)
{
    LINGLONG_TRACE("buildStagePullDependency_rejects_incompatible_base_and_runtime");

    auto runtimeRef = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    auto configuredBaseRef = package::Reference::parse("main:org.example.base/23.0.0.0/x86_64");
    ASSERT_TRUE(runtimeRef.has_value());
    ASSERT_TRUE(configuredBaseRef.has_value());

    auto project = builderProject("org.deepin.runtime/23.0.0.0/x86_64");
    project.base = "org.example.base/23.0.0/x86_64";
    builder::Builder builder(std::move(project),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });

    {
        InSequence seq;
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*runtimeRef));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*configuredBaseRef));
    }
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.base = "org.deepin.base/23.0.0.0/x86_64";
    EXPECT_CALL(*m_repo, getLayerItem(*runtimeRef, "binary"))
      .Times(1)
      .WillRepeatedly([runtimeItem](const package::Reference &, const std::string &)
                        -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
          return runtimeItem;
      });
    EXPECT_CALL(*m_repo, getLayerDir(_, "binary"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });
    EXPECT_CALL(*m_repo, mergeModules()).Times(0);

    auto result = builder.buildStagePullDependency();
    ASSERT_FALSE(result.has_value());
    EXPECT_THAT(result.error().message(),
                testing::HasSubstr("base is not compatible with runtime"));
}

TEST_F(PullDependencyTest, buildStagePullDependency_accepts_semantically_compatible_base)
{
    LINGLONG_TRACE("buildStagePullDependency_accepts_semantically_compatible_base");

    auto runtimeRef = package::Reference::parse("main:org.deepin.runtime/23.0.0.0/x86_64");
    auto baseRef = package::Reference::parse("main:org.deepin.base/23.0.0.1/x86_64");
    ASSERT_TRUE(runtimeRef.has_value());
    ASSERT_TRUE(baseRef.has_value());

    auto project = builderProject("org.deepin.runtime/23.0.0/x86_64");
    project.base = "org.deepin.base/23.0.0/x86_64";
    builder::Builder builder(std::move(project),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(
      builder::BuilderBuildOptions{ .skipFetchSource = true, .skipPullDepend = true });

    {
        InSequence seq;
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*runtimeRef));
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*baseRef));
    }
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });

    api::types::v1::RepositoryCacheLayersItem runtimeItem;
    runtimeItem.info.base = "org.deepin.base/23.0.0/x86_64";
    EXPECT_CALL(*m_repo, getLayerItem(*runtimeRef, "binary"))
      .Times(1)
      .WillRepeatedly([runtimeItem](const package::Reference &, const std::string &)
                        -> utils::error::Result<api::types::v1::RepositoryCacheLayersItem> {
          return runtimeItem;
      });
    EXPECT_CALL(*m_repo, getLayerDir(_, "binary"))
      .Times(2)
      .WillRepeatedly([](const package::Reference &,
                         const std::string &) -> utils::error::Result<package::LayerDir> {
          return layerCached();
      });
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, false)).Times(0);
    EXPECT_CALL(*m_repo, getLayerItem(*baseRef, _)).Times(0);
    EXPECT_CALL(*m_repo, getModuleList(*baseRef))
      .WillOnce(Return(std::vector<std::string>{ "binary" }));
    EXPECT_CALL(*m_repo, getModuleList(*runtimeRef))
      .WillOnce(Return(std::vector<std::string>{ "binary" }));
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.project->base);
    ASSERT_TRUE(builder.project->runtime);
    EXPECT_EQ(*builder.project->base, "org.deepin.base/23.0.0/x86_64");
    EXPECT_EQ(*builder.project->runtime, "org.deepin.runtime/23.0.0/x86_64");
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "base")->resolved.reference,
              baseRef->toString());
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "runtime")->resolved.reference,
              runtimeRef->toString());
}

} // namespace
