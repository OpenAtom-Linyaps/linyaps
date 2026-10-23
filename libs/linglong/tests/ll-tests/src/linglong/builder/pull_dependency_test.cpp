// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/tempdir.h"
#define private public
#include "linglong/builder/linglong_builder.h"
#undef private
#include "linglong/api/types/v1/Generators.hpp"
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
#include <vector>

using namespace linglong;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace {

const auto currentArchitecture = package::Architecture::currentCPUArchitecture().toString();

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

std::string lockedReference(const package::Reference &reference, bool explicitArchitecture = true)
{
    return reference.channel + ":" + reference.id + "/" + reference.version.toString() + "/"
      + (explicitArchitecture ? reference.arch.toString() : "unknown");
}

std::string lockedReference(const utils::error::Result<package::Reference> &reference,
                            bool explicitArchitecture = true)
{
    return lockedReference(*reference, explicitArchitecture);
}

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

auto remotePackages(const std::vector<package::Reference> &references,
                    const std::string &module = "binary")
  -> utils::error::Result<repo::RemotePackages>
{
    std::vector<api::types::v1::PackageInfoV2> infos;
    for (const auto &reference : references) {
        api::types::v1::PackageInfoV2 info;
        info.arch = { reference.arch.toString() };
        info.channel = reference.channel;
        info.id = reference.id;
        info.packageInfoV2Module = module;
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
    project.base = "org.deepin.base/23.0.0.0/" + currentArchitecture;
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
                                          { { "binary" }, "main:org.deepin.base/23.0.0.0" },
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
                                          { { "binary" }, lockedReference(reference) },
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
    auto oldReference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto newReference =
      package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(oldReference);
    ASSERT_TRUE(newReference);
    auto project = builderProject();
    project.base = "org.deepin.base/24.0.0.0/" + currentArchitecture;
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
    const builder::LockedDependency base{ "org.deepin.base/23.0.0.0/" + currentArchitecture,
                                          { { "binary" }, lockedReference(oldReference) },
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
              lockedReference(oldReference));
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
    EXPECT_EQ(written->dependencies.front().resolved.reference, lockedReference(newReference));
    ASSERT_EQ(written->sources.size(), 1);
    EXPECT_EQ(written->sources.front().resolved.hash, hash);
    EXPECT_EQ(written->sources.front().source.url, source.url);
}

TEST_F(PullDependencyTest, SkipFetchWithoutLockRecordsDependenciesAndLeavesSourcesForNormalBuild)
{
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
    EXPECT_EQ(written->dependencies.front().resolved.reference, lockedReference(reference));
    EXPECT_TRUE(std::filesystem::exists(manualSource));
    auto next = builder::resolveProjectLockSources(project.sources, *written);
    ASSERT_TRUE(next) << next.error().message();
    EXPECT_TRUE(next->changed);
    ASSERT_EQ(next->requests.size(), 1);
    EXPECT_TRUE(std::holds_alternative<builder::GenerateSourceRequest>(next->requests.front()));
}

TEST_F(PullDependencyTest, FreshSourcesWaitForDependenciesBeforeCreatingLock)
{
    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
    EXPECT_EQ(written->dependencies.front().resolved.reference, lockedReference(baseRef));
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
    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
        EXPECT_EQ(written->dependencies.front().resolved.reference, lockedReference(baseRef));
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
    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(baseRef) << baseRef.error().message();
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    const builder::LockedDependency base{ *builder.project->base,
                                          { { "binary" }, lockedReference(baseRef) },
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
    EXPECT_EQ(updated->dependencies.front().resolved.reference, lockedReference(baseRef));
}

TEST_F(PullDependencyTest, LockedDependencyChangesFailBeforeFetching)
{
    const builder::LockedDependency base{ "org.deepin.base/23.0.0.0/" + currentArchitecture,
                                          { { "binary" }, "main:org.deepin.base/23.0.0.0" },
                                          "base" };
    const builder::LockedDependency runtime{
        "org.deepin.runtime/23.0.0.0/" + currentArchitecture,
        { { "binary" }, "main:org.deepin.runtime/23.0.0.0" },
        "runtime",
    };
    for (const auto &change : { "base", "runtime", "add-runtime", "remove-runtime" }) {
        SCOPED_TRACE(change);
        auto project = builderProject(runtime.requested);
        auto lock = builder::completeProjectLock({}, base, runtime);
        if (std::string(change) == "base") {
            project.base = "org.deepin.base/24.0.0.0/" + currentArchitecture;
        } else if (std::string(change) == "runtime") {
            project.runtime = "org.deepin.runtime/24.0.0.0/" + currentArchitecture;
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
                                               { { "binary", "develop" },
                                                 lockedReference(reference) },
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary" }, lockedReference(reference) },
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
    auto oldBase =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto runtime =
      package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(oldBase);
    ASSERT_TRUE(runtime);
    for (const bool locked : { false, true }) {
        SCOPED_TRACE(locked);
        auto project = builderProject("org.deepin.runtime/23.0.0.0/" + currentArchitecture);
        project.base.reset();
        builder::Builder builder(project,
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        const builder::LockedDependency baseEntry{ "org.deepin.base/23.0.0.0/"
                                                     + currentArchitecture,
                                                   { { "binary" }, lockedReference(oldBase) },
                                                   "base" };
        const builder::LockedDependency runtimeEntry{ *project.runtime,
                                                      { { "binary" }, lockedReference(runtime) },
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

    auto oldBase = package::Reference::parse("main:org.test.Base/1.0.1/" + currentArchitecture);
    auto newBase = package::Reference::parse("main:org.test.Base/1.0.2/" + currentArchitecture);
    auto runtime = package::Reference::parse("main:org.test.Runtime/2.0.0/" + currentArchitecture);
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
                                                       { { "binary" }, lockedReference(oldBase) },
                                                       "base" };
            const builder::LockedDependency runtimeEntry{ *project.runtime,
                                                          { { "binary" },
                                                            lockedReference(runtime) },
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
            EXPECT_EQ(writtenBase->resolved.reference, lockedReference(expectedBase, false));
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
    auto oldBase =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto oldRuntime =
      package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
    auto newBase =
      package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
    auto newRuntime =
      package::Reference::parse("main:org.deepin.runtime/24.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(oldBase);
    ASSERT_TRUE(oldRuntime);
    ASSERT_TRUE(newBase);
    ASSERT_TRUE(newRuntime);
    for (const int scenario : { 0, 1, 2 }) {
        SCOPED_TRACE(scenario);
        const bool existingLock = scenario != 0;
        const bool reuseBase = scenario == 2;
        const auto &expectedBase = reuseBase ? *oldBase : *newBase;
        auto project = builderProject("org.deepin.runtime/24.0.0.0/" + currentArchitecture);
        project.base.reset();
        builder::Builder builder(project,
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (m_tmpDir.path() / "linglong.yaml").string();
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        if (existingLock) {
            const builder::LockedDependency baseEntry{ "org.deepin.base/23.0.0.0/"
                                                         + currentArchitecture,
                                                       { { "binary" }, lockedReference(oldBase) },
                                                       "base" };
            const builder::LockedDependency runtimeEntry{
                "org.deepin.runtime/23.0.0.0/" + currentArchitecture,
                { { "binary" }, lockedReference(oldRuntime) },
                "runtime"
            };
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
        runtimeItem.info.base = reuseBase ? "org.deepin.base/23.0.0.0/" + currentArchitecture
                                          : "org.deepin.base/24.0.0.0/" + currentArchitecture;
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
        EXPECT_EQ(written->dependencies[0].resolved.reference, lockedReference(expectedBase));
        EXPECT_EQ(written->dependencies[1].requested, *project.runtime);
        EXPECT_EQ(written->dependencies[1].resolved.reference, lockedReference(newRuntime));
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
        const builder::LockedDependency base{ "org.test.Base/1.0.0"
                                                + (mode == 2 ? "/" + currentArchitecture : ""),
                                              { { "binary" }, "main:org.test.Base/1.0.0.0" },
                                              "base" };
        const builder::LockedDependency runtime{ "org.test.Runtime/2.0.0",
                                                 { { "binary" }, "main:org.test.Runtime/2.0.0.0" },
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

        std::string expectedBase = base.requested;
        if (mode == 2) {
            const auto architecture = package::Architecture::currentCPUArchitecture().toString();
            for (const auto &id : { "org.test.Base", "org.test.Runtime" }) {
                const auto input = workingDir.path() / id;
                std::filesystem::create_directories(input / "files");
                const std::string version =
                  std::string(id) == "org.test.Base" ? "1.0.0.0" : "2.0.0.0";
                api::types::v1::PackageInfoV2 info{ .arch = { architecture },
                                                    .base = expectedBase,
                                                    .channel = "main",
                                                    .id = id,
                                                    .kind = "runtime",
                                                    .packageInfoV2Module = "binary",
                                                    .version = version };
                std::ofstream(input / "info.json") << nlohmann::json(info).dump();
                std::ofstream(input / "files/payload") << id;
                ASSERT_TRUE((*repo)->importLayerDir(package::LayerDir(input)));
            }
            ASSERT_TRUE((*repo)->mergeModules());
            auto context = builder.buildContext.resolve(project,
                                                        builder.directories.buildOutput(),
                                                        *builder.projectLock);
            ASSERT_TRUE(context) << context.error().message();
            ASSERT_TRUE(builder.buildContext.getRuntimeLayer());
            EXPECT_EQ(builder.buildContext.getRuntimeLayer()->getCachedItem().info.base,
                      expectedBase);
        }

        auto committed = builder.commitToLocalRepo();
        ASSERT_TRUE(committed) << committed.error().message();
        for (const auto &module : builder.packageModules) {
            const auto layer = (*repo)->getLayerDir(*reference, module);
            ASSERT_TRUE(layer);
            const auto info = layer->info();
            ASSERT_TRUE(info);
            EXPECT_EQ(info->base, expectedBase);
            EXPECT_EQ(info->runtime, project.runtime);
        }
        EXPECT_EQ(builder.project->base, project.base);
        EXPECT_EQ(builder.project->runtime, project.runtime);
    }
}

TEST_F(PullDependencyTest, RejectsDependencyResolutionOutsideDeclaredReference)
{
    LINGLONG_TRACE("validate dependency resolution before recording a lock entry");

    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const std::string requested = "main:org.test.Base/1.0.0.0/x86_64";
    EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
    for (const auto &resolved : { "main:org.test.Other/1.0.0.0/x86_64",
                                  "other:org.test.Base/1.0.0.0/x86_64",
                                  "main:org.test.Base/9.0.0.0/x86_64",
                                  "main:org.test.Base/1.0.0.0/arm64" }) {
        SCOPED_TRACE(resolved);
        auto reference = package::Reference::parse(resolved);
        ASSERT_TRUE(reference) << reference.error().message();
        auto entry =
          builder.recordProjectDependency(builder::ResolveDependencyRequest{ requested, "base" },
                                          *reference);
        ASSERT_FALSE(entry);
        EXPECT_THAT(entry.error().message(), testing::HasSubstr("does not match its declaration"));
    }
    testing::Mock::VerifyAndClearExpectations(m_repo.get());

    auto reference = package::Reference::parse("main:org.test.Base/1.0.0.1/" + currentArchitecture);
    ASSERT_TRUE(reference);
    EXPECT_CALL(*m_repo, getModuleList(*reference))
      .WillOnce(Return(std::vector<std::string>{ "binary" }));
    auto entry = builder.recordProjectDependency(
      builder::ResolveDependencyRequest{ "org.test.Base/1.0.0", "base" },
      *reference);
    ASSERT_TRUE(entry) << entry.error().message();
    EXPECT_EQ(entry->resolved.reference, "main:org.test.Base/1.0.0.1/unknown");
    const auto manifest = m_tmpDir.path() / "linglong.yaml";
    ASSERT_TRUE(
      builder::writeCompleteProjectLock(manifest, builder::completeProjectLock({}, *entry)));
    EXPECT_TRUE(builder::readProjectLock(builder::projectLockPath(manifest)));
}

TEST_F(PullDependencyTest, PreservesLegacyRuntimeModuleInLock)
{
    const auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary", "develop" }, lockedReference(reference) },
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const builder::LockedDependency entry{ *builder.project->base,
                                           { { "binary" }, lockedReference(reference) },
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
    auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
                                                            lockedReference(reference) },
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

TEST_F(PullDependencyTest, OptionalDevelopFailuresPreserveLockAndRetry)
{
    LINGLONG_TRACE("optional develop failures do not become a permanent binary-only lock");
    utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                          (m_tmpDir.path() / "cache").string());
    const auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);

    for (const bool update : { false, true }) {
        for (const bool failLookup : { false, true }) {
            SCOPED_TRACE(update);
            SCOPED_TRACE(failLookup);
            const auto directory = m_tmpDir.path()
              / (std::string(update ? "update-" : "generate-")
                 + (failLookup ? "lookup" : "download"));
            ASSERT_TRUE(std::filesystem::create_directory(directory));
            builder::Builder builder(builderProject(),
                                     directory,
                                     *m_repo,
                                     containerBuilder(),
                                     builderConfig());
            builder.projectYamlFile = (directory / "linglong.yaml").string();
            const auto path = builder::projectLockPath(builder.projectYamlFile);
            std::optional<std::string> previous;
            if (update) {
                const builder::LockedDependency base{ *builder.project->base,
                                                      { { "binary", "develop" },
                                                        lockedReference(reference) },
                                                      "base" };
                ASSERT_TRUE(
                  builder::writeCompleteProjectLock(builder.projectYamlFile,
                                                    builder::completeProjectLock({}, base)));
                auto contents = utils::readFile(path);
                ASSERT_TRUE(contents);
                previous = *contents;
            }
            const auto mode =
              update ? builder::ProjectLockMode::Update : builder::ProjectLockMode::Generate;

            EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*reference));
            expectCachedModule(*reference, "binary");
            EXPECT_CALL(*m_repo, getLayerDir(*reference, "develop"))
              .WillRepeatedly(
                [&](const auto &, const auto &) -> utils::error::Result<package::LayerDir> {
                    return LINGLONG_ERR("not installed");
                });
            EXPECT_CALL(*m_repo, mergeModules()).Times(testing::AtMost(1)).WillRepeatedly([] {
                return utils::error::Result<void>{};
            });
            EXPECT_CALL(*m_repo, getModuleList(*reference))
              .Times(testing::AtMost(1))
              .WillRepeatedly(Return(std::vector<std::string>{ "binary" }));
            if (failLookup) {
                EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
                  .WillOnce(Return(LINGLONG_ERR("metadata request failed")));
                EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
            } else {
                EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
                  .WillOnce([&](const auto &, const auto &) {
                      return remotePackages({ *reference }, "develop");
                  });
                EXPECT_CALL(*m_repo, pull(_, _, "develop"))
                  .WillOnce(Return(LINGLONG_ERR("download interrupted")));
            }

            auto failed = builder.processProjectLock(mode);
            ASSERT_FALSE(failed);
            EXPECT_THAT(
              failed.error().message(),
              testing::HasSubstr("failed to fetch develop module of " + reference->toString()));
            if (previous) {
                auto contents = utils::readFile(path);
                ASSERT_TRUE(contents);
                EXPECT_EQ(*contents, *previous);
            } else {
                EXPECT_FALSE(std::filesystem::exists(path));
            }
            testing::Mock::VerifyAndClearExpectations(m_repo.get());

            // Retrying after the repository recovers must fetch and lock develop.
            EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(*reference));
            expectCachedModule(*reference, "binary");
            EXPECT_CALL(*m_repo, getLayerDir(*reference, "develop"))
              .WillOnce(Return(LINGLONG_ERR("not installed")))
              .WillOnce(Return(LINGLONG_ERR("not installed")))
              .WillOnce(Return(layerCached()));
            EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
              .WillOnce([&](const auto &, const auto &) {
                  return remotePackages({ *reference }, "develop");
              });
            EXPECT_CALL(*m_repo, pull(_, _, "develop"))
              .WillOnce(Return(utils::error::Result<void>{}));
            EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
            EXPECT_CALL(*m_repo, getModuleList(*reference))
              .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));

            auto retried = builder.processProjectLock(mode);
            ASSERT_TRUE(retried) << retried.error().message();
            auto written = builder::readProjectLock(path);
            ASSERT_TRUE(written) << written.error().message();
            ASSERT_EQ(written->dependencies.size(), 1);
            EXPECT_EQ(written->dependencies.front().resolved.modules,
                      (std::vector<std::string>{ "binary", "develop" }));
            testing::Mock::VerifyAndClearExpectations(m_repo.get());
        }
    }
}

TEST_F(PullDependencyTest, OptionalDevelopUsesExactVersionAndSelectedRepository)
{
    LINGLONG_TRACE("do not combine optional modules across versions or repositories");
    const auto reference =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);
    ASSERT_TRUE(newer);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    const auto primary = refWithRepo(*reference, "primary");
    auto binary = remotePackages({ *reference })->getRepoPackages().front().second.front();
    auto newerDevelop =
      remotePackages({ *newer }, "develop")->getRepoPackages().front().second.front();
    auto otherDevelop = binary;
    otherDevelop.packageInfoV2Module = "develop";

    for (const bool selected : { false, true }) {
        SCOPED_TRACE(selected);
        std::optional<package::ReferenceWithRepo> remote =
          selected ? std::optional{ primary } : std::nullopt;
        EXPECT_CALL(*m_repo, getLayerDir(*reference, "develop"))
          .WillOnce(Return(LINGLONG_ERR("not installed")));
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _))
          .WillOnce([&](const package::FuzzyReference &fuzzy,
                        const std::optional<api::types::v1::Repo> &specified)
                      -> utils::error::Result<repo::RemotePackages> {
              EXPECT_EQ(fuzzy.toString(), reference->toString());
              EXPECT_EQ(specified.has_value(), selected);
              if (specified) {
                  EXPECT_EQ(specified->name, primary.repo.name);
              }
              repo::RemotePackages candidates;
              candidates.addPackages(primary.repo, { binary, newerDevelop });
              if (!specified) {
                  candidates.addPackages(refWithRepo(*reference, "secondary").repo,
                                         { otherDevelop });
              }
              return utils::error::Result<repo::RemotePackages>{ std::move(candidates) };
          });
        EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
        auto result = builder.ensureDependencyModule(*reference, "develop", remote, true);
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_FALSE(*result);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, ResolvedBinaryFallbackFetchesDevelopForFinalReference)
{
    LINGLONG_TRACE("fetch develop for the local fallback version");
    auto local = package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto newer = package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
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
          return remotePackages({ *newer, *local }, "develop");
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

TEST_F(PullDependencyTest, LockedDependencyFetchesExactVersionForSelectedArchitecture)
{
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    for (const std::string architecture : { "unknown", "arm64" }) {
        SCOPED_TRACE(architecture);
        const builder::LockedDependency entry{ "org.test.Base/1.0.0",
                                               { { "binary", "develop" },
                                                 "main:org.test.Base/1.0.0.0/" + architecture },
                                               "base" };
        const auto original = nlohmann::json(entry);
        auto reference = package::Reference::parse(
          "main:org.test.Base/1.0.0.0/"
          + (architecture == "unknown" ? currentArchitecture : architecture));
        ASSERT_TRUE(reference);
        builder::Builder builder(builderProject(),
                                 m_tmpDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
        expectCachedModule(*reference, "binary");
        expectCachedModule(*reference, "develop");
        auto result = builder.fetchProjectDependency(builder::LockedDependencyRequest{ entry });
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_EQ(*result, *reference);
        EXPECT_EQ(nlohmann::json(entry), original);
    }
}

TEST_F(PullDependencyTest, SharedLockDoesNotFallBackToAnotherArchitectureOrVersion)
{
    LINGLONG_TRACE("reject a missing locked dependency for the selected architecture");

    const builder::LockedDependency entry{ "org.test.Base/1.0.0",
                                           { { "binary" }, "main:org.test.Base/1.0.0.0/unknown" },
                                           "base" };
    const auto reference =
      package::Reference::parse("main:org.test.Base/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(reference);
    builder::Builder builder(builderProject(),
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    EXPECT_CALL(*m_repo, getLayerDir(*reference, "binary"))
      .WillOnce(Return(LINGLONG_ERR("missing module for build host")));
    auto result = builder.fetchProjectDependency(builder::LockedDependencyRequest{ entry });
    ASSERT_FALSE(result);
    EXPECT_THAT(result.error().message(), testing::HasSubstr(reference->toString()));
}

TEST_F(PullDependencyTest, ImplicitBaseUsesCurrentRuntimeMetadataArchitecture)
{
    auto project = builderProject("org.test.Runtime/2.0.0/arm64");
    project.base.reset();
    const builder::LockedDependency base{ "org.test.Base/1.0.0/arm64",
                                          { { "binary" }, "main:org.test.Base/1.0.0.0/arm64" },
                                          "base" };
    const builder::LockedDependency runtime{ *project.runtime,
                                             { { "binary" },
                                               "main:org.test.Runtime/2.0.0.0/arm64" },
                                             "runtime" };
    const auto baseReference = package::Reference::parse("main:org.test.Base/1.0.0.0/arm64");
    const auto runtimeReference = package::Reference::parse("main:org.test.Runtime/2.0.0.0/arm64");
    ASSERT_TRUE(baseReference);
    ASSERT_TRUE(runtimeReference);
    builder::Builder builder(project,
                             m_tmpDir.path(),
                             *m_repo,
                             containerBuilder(),
                             builderConfig());
    builder.projectLock = builder::completeProjectLock({}, base, runtime);
    const auto original = nlohmann::json(*builder.projectLock);
    builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true, .locked = true });
    api::types::v1::RepositoryCacheLayersItem item;
    item.info.base = "org.test.Base/1.0.0/arm64";
    EXPECT_CALL(*m_repo, getLayerItem(*runtimeReference, "binary")).WillOnce(Return(item));
    expectCachedModule(*baseReference, "binary");
    expectCachedModule(*runtimeReference, "binary");
    EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
    EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result) << result.error().message();
    EXPECT_EQ(nlohmann::json(*builder.projectLock), original);
    EXPECT_FALSE(builder.project->base);
}

TEST_F(PullDependencyTest, DerivedBasePreservesDeclarationConstraints)
{
    const std::string other = currentArchitecture == "arm64" ? "x86_64" : "arm64";
    const std::vector<std::pair<std::string, std::string>> declarations{
        { "org.test.Base", "unknown" },
        { "org.test.Base/unknown", "unknown" },
        { "org.test.Base/unknown/" + other, other },
        { "org.test.Base/1.0.0", "unknown" },
        { "org.test.Base/1.0.0/unknown", "unknown" },
        { "org.test.Base/1.0.0/" + other, other }
    };
    for (const auto &[declaration, architecture] : declarations) {
        SCOPED_TRACE(declaration);
        TempDir workingDir;
        auto project = builderProject("org.test.Runtime/2.0.0/" + other);
        project.base.reset();
        const auto base = package::Reference::parse(
          "main:org.test.Base/1.0.0.0/"
          + (architecture == "unknown" ? currentArchitecture : architecture));
        const auto runtime = package::Reference::parse("main:org.test.Runtime/2.0.0.0/" + other);
        ASSERT_TRUE(base);
        ASSERT_TRUE(runtime);
        builder::Builder builder(project,
                                 workingDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
        builder.setBuildOptions(builder::BuilderBuildOptions{ .skipPullDepend = true });

        EXPECT_CALL(*m_repo, clearReferenceLocal(_, true))
          .WillOnce(Return(*runtime))
          .WillOnce(Return(*base));
        EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
        EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).Times(0);
        EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);
        for (const auto &reference : { *base, *runtime }) {
            expectCachedModule(reference, "binary");
            expectCachedModule(reference, "develop");
            EXPECT_CALL(*m_repo, getModuleList(reference))
              .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));
        }
        api::types::v1::RepositoryCacheLayersItem item;
        item.info.base = declaration;
        EXPECT_CALL(*m_repo, getLayerItem(*runtime, "binary")).WillOnce(Return(item));
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));

        auto result = builder.buildStagePullDependency();
        ASSERT_TRUE(result) << result.error().message();
        auto lock = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(lock) << lock.error().message();
        auto lockedBase = builder::lockedDependency(*lock, "base");
        ASSERT_TRUE(lockedBase);
        EXPECT_EQ(lockedBase->requested, declaration);
        EXPECT_EQ(lockedBase->resolved.reference, "main:org.test.Base/1.0.0.0/" + architecture);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());

        builder.setBuildOptions(
          builder::BuilderBuildOptions{ .skipPullDepend = true, .locked = true });
        EXPECT_CALL(*m_repo, clearReferenceLocal(_, _)).Times(0);
        EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);
        EXPECT_CALL(*m_repo, getModuleList(_)).Times(0);
        EXPECT_CALL(*m_repo, getLayerItem(*runtime, "binary")).WillOnce(Return(item));
        EXPECT_CALL(*m_repo, mergeModules()).WillOnce(Return(utils::error::Result<void>{}));
        for (const auto &reference : { *base, *runtime }) {
            expectCachedModule(reference, "binary");
            expectCachedModule(reference, "develop");
        }
        result = builder.buildStagePullDependency();
        ASSERT_TRUE(result) << result.error().message();
        EXPECT_EQ(nlohmann::json(*builder.projectLock), nlohmann::json(*lock));
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, CrossBuildGeneratesAndUpdatesLock)
{
    auto base = package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
                                                       "main:org.deepin.base/23.0.0.0" },
                                                     "base" };
            auto oldLock = builder::completeProjectLock({}, oldBase);
            ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile, oldLock));
        }

        expectFreshBase(*base);
        auto result = builder.processProjectLock(mode);
        ASSERT_TRUE(result) << result.error().message();
        auto written = builder::readProjectLock(builder::projectLockPath(builder.projectYamlFile));
        ASSERT_TRUE(written);
        auto baseEntry = builder::lockedDependency(*written, "base");
        ASSERT_TRUE(baseEntry);
        EXPECT_EQ(baseEntry->requested, *project.base);
        EXPECT_EQ(baseEntry->resolved.reference, lockedReference(base, false));
        EXPECT_EQ(builder.project->package.architecture, "arm64");
        EXPECT_EQ(builder.project->base, project.base);
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, PrepareLoadsSharedLockIndependentlyOfTargetArchitecture)
{
    for (const auto &target : { "x86_64", "arm64" }) {
        SCOPED_TRACE(target);
        TempDir workingDir;
        // Preparation validates package metadata after loading the lock.
        // An empty package keeps this check independent of installed builder helpers.
        api::types::v1::BuilderProject project;
        project.package.architecture = target;
        builder::Builder builder(project,
                                 workingDir.path(),
                                 *m_repo,
                                 containerBuilder(),
                                 builderConfig());
        builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
        const builder::LockedDependency base{ "org.deepin.base/23.0.0.0",
                                              { { "binary" }, "main:org.deepin.base/23.0.0.0" },
                                              "base" };
        ASSERT_TRUE(builder::writeCompleteProjectLock(builder.projectYamlFile,
                                                      builder::completeProjectLock({}, base)));
        auto result = builder.buildStagePrepare();
        ASSERT_FALSE(result);
        EXPECT_THAT(result.error().message(), testing::HasSubstr("invalid project info"));
        ASSERT_TRUE(builder.projectLock);
        EXPECT_EQ(builder.projectLock->dependencies.front().resolved.reference,
                  base.resolved.reference);
    }
}

TEST_F(PullDependencyTest, FailedSharedLockUpdatePreservesOldLock)
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
    const builder::LockedDependency base{ *project.base,
                                          { { "binary" }, "main:org.deepin.base/23.0.0.0" },
                                          "base" };
    auto lock = builder::completeProjectLock({}, base);
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

    EXPECT_CALL(*m_repo, getModuleList(*reference))
      .WillOnce(Return(std::vector<std::string>{ "binary", "develop" }));
    auto entry =
      builder.recordProjectDependency(builder::ResolveDependencyRequest{ requested, "base" },
                                      *result);
    ASSERT_TRUE(entry) << entry.error().message();
    EXPECT_EQ(entry->requested, requested);
    EXPECT_EQ(entry->resolved.reference, reference->toString());
}

TEST_F(PullDependencyTest, CrossBuildPreservesDependencyArchitectures)
{
    LINGLONG_TRACE("resolve and reuse host dependencies for a cross build");

    auto base = package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto runtime =
      package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
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
                                                         lockedReference(base, false) },
                                                       "base" };
            std::optional<builder::LockedDependency> runtimeEntry;
            if (withRuntime) {
                runtimeEntry = builder::LockedDependency{ *project.runtime,
                                                          { { "binary", "develop" },
                                                            lockedReference(runtime, false) },
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
            EXPECT_EQ(writtenBase->resolved.reference, lockedReference(base, false));
            if (withRuntime) {
                auto writtenRuntime = builder::lockedDependency(*written, "runtime");
                ASSERT_TRUE(writtenRuntime);
                EXPECT_EQ(writtenRuntime->resolved.reference, lockedReference(runtime, false));
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

    auto local = package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    auto remote = package::Reference::parse("main:org.deepin.base/24.0.0.0/" + currentArchitecture);
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
    EXPECT_EQ(baseEntry->resolved.reference, lockedReference(local, false));
    EXPECT_EQ(builder.project->package.architecture, "arm64");
}

TEST_F(PullDependencyTest, clearDependency_useRemoteFalse_returns_local_ref)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*ref)));

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       false);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->toString(), "main:org.example.test/1.0.0.0/" + currentArchitecture);
}

TEST_F(PullDependencyTest, clearDependency_useRemoteFalse_not_found_returns_error)
{
    LINGLONG_TRACE("clearDependency_useRemoteFalse_not_found_returns_error");

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));

    auto result =
      builder::detail::clearDependency("org.example.missing/1.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       false);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, clearDependency_returns_local_variant)
{
    auto localRef =
      package::Reference::parse("main:org.example.test/3.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    auto result =
      builder::detail::clearDependency("org.example.test/2.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->version.toString(), "3.0.0.0");
}

TEST_F(PullDependencyTest, clearDependency_exact_version_returns_local_variant)
{
    auto localRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(result->second);
    EXPECT_FALSE(result->first);
    EXPECT_EQ(result->second->version.toString(), "1.0.0.0");
}

TEST_F(PullDependencyTest, clearDependency_module_missing_returns_remote_variant)
{
    LINGLONG_TRACE("clearDependency_module_missing_returns_remote_variant");

    auto localRef =
      package::Reference::parse("main:org.example.test/3.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, getLayerDir(_, "develop")).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef), "custom")));

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       true,
                                       "develop");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(result->first);
    EXPECT_FALSE(result->second);
    EXPECT_EQ(result->first->repo.name, "custom");
}

TEST_F(PullDependencyTest, useRemoteTrue_prefers_remote_when_newer)
{
    auto localRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
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
    auto localRef =
      package::Reference::parse("main:org.example.test/3.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

    auto result = builder::detail::pullDependency("org.example.test/2.0.0.0/" + currentArchitecture,
                                                  *m_repo,
                                                  "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->version.toString(), "3.0.0.0");
}

TEST_F(PullDependencyTest, useRemoteTrue_falls_back_to_local_when_remote_pull_fails)
{
    LINGLONG_TRACE("useRemoteTrue_falls_back_to_local_when_remote_pull_fails");

    auto localRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
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
    auto localRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
    auto remoteRef =
      package::Reference::parse("main:org.example.test/2.0.0.0/" + currentArchitecture);
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
    auto localRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).Times(0);

    auto result =
      builder::detail::clearDependency("org.example.test/1.0.0.0/" + currentArchitecture,
                                       *m_repo,
                                       true);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(result->first);
    ASSERT_TRUE(result->second);
    EXPECT_EQ(result->second->toString(), "main:org.example.test/1.0.0.0/" + currentArchitecture);
}

TEST_F(PullDependencyTest, useRemoteTrue_only_local_exists)
{
    LINGLONG_TRACE("useRemoteTrue_only_local_exists");

    auto localRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(localRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(std::move(*localRef)));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("not found")));

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).Times(0);

    auto result = builder::detail::pullDependency("org.example.test", *m_repo, "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(result->toString(), "main:org.example.test/1.0.0.0/" + currentArchitecture);
}

TEST_F(PullDependencyTest, useRemoteTrue_only_remote_exists)
{
    LINGLONG_TRACE("useRemoteTrue_only_remote_exists");

    auto remoteRef =
      package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(remoteRef.has_value());

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_))
      .WillOnce(Return(refWithRepo(std::move(*remoteRef))));

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullDependency("org.example.test/1.0.0.0/" + currentArchitecture,
                                                  *m_repo,
                                                  "binary");
    ASSERT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, useRemoteTrue_neither_exists_returns_error)
{
    LINGLONG_TRACE("useRemoteTrue_neither_exists_returns_error");

    EXPECT_CALL(*m_repo, clearReferenceLocal(_, true)).WillOnce(Return(LINGLONG_ERR("not found")));
    EXPECT_CALL(*m_repo, latestRemoteReference(_)).WillOnce(Return(LINGLONG_ERR("not found")));

    auto result =
      builder::detail::pullDependency("org.example.missing/1.0.0.0/" + currentArchitecture,
                                      *m_repo,
                                      "binary");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, invalid_fuzzy_ref_returns_error)
{
    auto result = builder::detail::pullDependency("::invalid///ref", *m_repo, "binary");
    EXPECT_FALSE(result.has_value());
}

TEST_F(PullDependencyTest, pullResolvedRef_cached_module_returns_ok)
{
    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(ref.has_value()) << ref.error().message();

    EXPECT_CALL(*m_repo, getLayerDir(_, _)).WillOnce(Return(layerCached()));

    auto result = builder::detail::pullResolvedRef(refWithRepo(std::move(*ref)), *m_repo, "binary");
    EXPECT_TRUE(result.has_value()) << result.error().message();
}

TEST_F(PullDependencyTest, pullResolvedRef_not_cached_triggers_pull)
{
    LINGLONG_TRACE("pullResolvedRef_not_cached_triggers_pull");

    auto ref = package::Reference::parse("main:org.example.test/1.0.0.0/" + currentArchitecture);
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

TEST_F(PullDependencyTest, buildStagePullDependency_allows_unavailable_develop)
{
    LINGLONG_TRACE("buildStagePullDependency_allows_unavailable_develop");

    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
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
    EXPECT_CALL(*m_repo, matchRemoteByPriority(_, _)).WillOnce([&](const auto &, const auto &) {
        return remotePackages({ *baseRef });
    });
    EXPECT_CALL(*m_repo, pull(_, _, _)).Times(0);

    auto result = builder.buildStagePullDependency();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    ASSERT_TRUE(builder.project->base);
    EXPECT_EQ(builder.project->base, builderProject().base);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_TRUE(builder.projectLock->sources.empty());
    ASSERT_EQ(builder.projectLock->dependencies.size(), 1);
    EXPECT_EQ(builder.projectLock->dependencies.front().resolved.reference,
              lockedReference(baseRef));
    EXPECT_FALSE(builder.pendingSources);
    EXPECT_TRUE(std::filesystem::exists(builder::projectLockPath(builder.projectYamlFile)));
}

TEST_F(PullDependencyTest, buildStagePullDependency_resolves_base_from_runtime)
{
    LINGLONG_TRACE("buildStagePullDependency_resolves_base_from_runtime");

    auto runtimeRef =
      package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(runtimeRef.has_value());
    ASSERT_TRUE(baseRef.has_value());

    auto project = builderProject("org.deepin.runtime/23.0.0.0/" + currentArchitecture);
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
    runtimeItem.info.base = "org.deepin.base/23.0.0.0/" + currentArchitecture;
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
    EXPECT_EQ(*builder.project->runtime, "org.deepin.runtime/23.0.0.0/" + currentArchitecture);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "base")->resolved.reference,
              lockedReference(baseRef));
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "runtime")->resolved.reference,
              lockedReference(runtimeRef));
}

TEST_F(PullDependencyTest, buildStagePullDependency_rejects_incompatible_base_and_runtime)
{
    LINGLONG_TRACE("buildStagePullDependency_rejects_incompatible_base_and_runtime");

    for (const std::string suffix : { std::string{}, "/23.0.0.0/" + currentArchitecture }) {
        SCOPED_TRACE(suffix);
        auto runtimeRef =
          package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
        auto configuredBaseRef =
          package::Reference::parse("main:org.example.base/23.0.0.0/" + currentArchitecture);
        ASSERT_TRUE(runtimeRef.has_value());
        ASSERT_TRUE(configuredBaseRef.has_value());

        auto project = builderProject("org.deepin.runtime/23.0.0.0/" + currentArchitecture);
        project.base = "org.example.base/23.0.0/" + currentArchitecture;
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
        runtimeItem.info.base = "org.deepin.base" + suffix;
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
        testing::Mock::VerifyAndClearExpectations(m_repo.get());
    }
}

TEST_F(PullDependencyTest, buildStagePullDependency_accepts_semantically_compatible_base)
{
    LINGLONG_TRACE("buildStagePullDependency_accepts_semantically_compatible_base");

    auto runtimeRef =
      package::Reference::parse("main:org.deepin.runtime/23.0.0.0/" + currentArchitecture);
    auto baseRef =
      package::Reference::parse("main:org.deepin.base/23.0.0.1/" + currentArchitecture);
    ASSERT_TRUE(runtimeRef.has_value());
    ASSERT_TRUE(baseRef.has_value());

    auto project = builderProject("org.deepin.runtime/23.0.0/" + currentArchitecture);
    project.base = "org.deepin.base/23.0.0/" + currentArchitecture;
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
    runtimeItem.info.base = "org.deepin.base/23.0.0/" + currentArchitecture;
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
    EXPECT_EQ(*builder.project->base, "org.deepin.base/23.0.0/" + currentArchitecture);
    EXPECT_EQ(*builder.project->runtime, "org.deepin.runtime/23.0.0/" + currentArchitecture);
    ASSERT_TRUE(builder.projectLock);
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "base")->resolved.reference,
              lockedReference(baseRef));
    EXPECT_EQ(builder::lockedDependency(*builder.projectLock, "runtime")->resolved.reference,
              lockedReference(runtimeRef));
}

} // namespace
