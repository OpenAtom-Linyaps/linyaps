// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/api/types/v1/BuilderProjectLock.hpp"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/builder/project_lock.h"
#include "linglong/builder/source_cache.h"
#include "linglong/builder/source_fetcher.h"
#include "linglong/package/architecture.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/env.h"
#include "linglong/utils/serialize/yaml.h"
#include "linglong/utils/temporary_directory.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace linglong::builder {

enum class SourcePreparationMode { UseOrGenerate, Locked, Generate, Update };

struct FetchedProjectSources
{
    std::vector<api::types::v1::BuilderSourceLockEntry> entries;
    bool changed;
};

static auto
resolveAndFetchProjectSources(const std::vector<api::types::v1::BuilderProjectSource> &sources,
                              const std::filesystem::path &cache,
                              const std::filesystem::path &destination,
                              bool offline,
                              const std::optional<api::types::v1::BuilderProjectLock> &existingLock)
  -> utils::error::Result<FetchedProjectSources>
{
    LINGLONG_TRACE("plan and fetch test project sources");

    auto plan = resolveProjectLockSources(sources, existingLock);
    if (!plan) {
        return LINGLONG_ERR(plan);
    }

    auto entries = fetchProjectSources(*plan, cache, destination, offline);
    if (!entries) {
        return LINGLONG_ERR(entries);
    }

    return FetchedProjectSources{ plan->changed ? std::move(*entries) : existingLock->sources,
                                  plan->changed };
}

static auto verifyAndFetchProjectSources(
  const std::vector<api::types::v1::BuilderProjectSource> &sources,
  const std::filesystem::path &cache,
  const std::filesystem::path &destination,
  bool offline,
  const api::types::v1::BuilderProjectLock &lock) -> utils::error::Result<void>
{
    LINGLONG_TRACE("verify and fetch test project sources");

    auto plan = verifyProjectLockSources(sources, lock);
    if (!plan) {
        return LINGLONG_ERR(plan);
    }

    auto entries = fetchProjectSources(*plan, cache, destination, offline);
    if (!entries) {
        return LINGLONG_ERR(entries);
    }

    return LINGLONG_OK;
}

static auto prepareSourcesFromManifest(
  const std::vector<api::types::v1::BuilderProjectSource> &sources,
  const std::filesystem::path &manifest,
  const std::filesystem::path &cache,
  SourcePreparationMode mode) -> utils::error::Result<FetchedProjectSources>
{
    LINGLONG_TRACE("prepare test sources from manifest");

    // Command preconditions are covered by Builder tests; select the source resolution input here.
    std::optional<api::types::v1::BuilderProjectLock> loaded;
    const auto path = projectLockPath(manifest);
    if (mode == SourcePreparationMode::Locked
        || (mode == SourcePreparationMode::UseOrGenerate && std::filesystem::exists(path))) {
        auto lock = readProjectLock(path);
        if (!lock) {
            return LINGLONG_ERR(lock);
        }
        loaded = std::move(*lock);
    }

    auto temporaryDestination = utils::TemporaryDirectory::create("fetch-");
    if (!temporaryDestination) {
        return LINGLONG_ERR(temporaryDestination);
    }
    if (mode == SourcePreparationMode::Locked) {
        auto verified = verifyAndFetchProjectSources(sources,
                                                     cache,
                                                     temporaryDestination->path(),
                                                     false,
                                                     *loaded);
        if (!verified) {
            return LINGLONG_ERR(verified);
        }
        return FetchedProjectSources{ loaded->sources, false };
    }

    return resolveAndFetchProjectSources(sources,
                                         cache,
                                         temporaryDestination->path(),
                                         false,
                                         loaded);
}

class ProjectLockSourceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        std::ofstream(manifest) << "version: 1\n";
        const auto commands = temporary.path() / "bin";
        std::filesystem::create_directories(commands);
        const auto downloader = commands / "wget";
        // Use local files as remote artifacts and record every download request.
        std::ofstream(downloader) << R"(#!/bin/sh
set -eu
test "$#" -eq 3
test "$2" = "-O"
printf '%s\n' "$1" >> "$LINGLONG_TEST_SOURCE_DOWNLOAD_LOG"
/bin/cp -- "$1" "$3"
)";
        std::error_code ec;
        std::filesystem::permissions(downloader, std::filesystem::perms::owner_all, ec);
        ASSERT_FALSE(ec) << ec.message();
        downloadLog =
          std::make_unique<utils::EnvironmentVariableGuard>("LINGLONG_TEST_SOURCE_DOWNLOAD_LOG",
                                                            downloads.string());
        const auto *originalPath = std::getenv("PATH");
        commandPath = std::make_unique<utils::EnvironmentVariableGuard>(
          "PATH",
          commands.string() + ":" + (originalPath ? originalPath : ""));
    }

    auto fileSource(const std::string &name, const std::string &contents)
      -> api::types::v1::BuilderProjectSource
    {
        const auto artifact = temporary.path() / name;
        std::ofstream(artifact) << contents;
        auto digest = fileHash(artifact);
        EXPECT_TRUE(digest.has_value());
        api::types::v1::BuilderProjectSource source;
        source.kind = "file";
        source.name = name;
        source.url = artifact.string();
        if (digest) {
            source.digest = *digest;
        }
        return source;
    }

    auto generateLock(const std::vector<api::types::v1::BuilderProjectSource> &sources)
      -> utils::error::Result<api::types::v1::BuilderProjectLock>
    {
        LINGLONG_TRACE("generate test project lock");
        auto generated =
          prepareSourcesFromManifest(sources, manifest, cache, SourcePreparationMode::Generate);
        if (!generated) {
            return LINGLONG_ERR(generated);
        }
        const LockedDependency base{ "org.test.Base",
                                     { { "binary" }, "main:org.test.Base/1.0.0" },
                                     "base" };
        auto lock = completeProjectLock(generated->entries, base);
        auto written = writeCompleteProjectLock(manifest, lock);
        if (!written) {
            return LINGLONG_ERR(written);
        }
        return lock;
    }

    auto downloadRequests() const -> std::vector<std::string>
    {
        std::ifstream log(downloads);
        std::vector<std::string> requests;
        std::string line;
        while (std::getline(log, line)) {
            requests.push_back(line);
        }
        return requests;
    }

    TempDir temporary;
    const std::filesystem::path manifest = temporary.path() / "linglong.yaml";
    const std::filesystem::path cache = temporary.path() / "cache";
    const std::filesystem::path downloads = temporary.path() / "downloads";
    std::unique_ptr<utils::EnvironmentVariableGuard> downloadLog;
    std::unique_ptr<utils::EnvironmentVariableGuard> commandPath;
};

class ProjectLockGitTest : public ProjectLockSourceTest
{
protected:
    void SetUp() override
    {
        ProjectLockSourceTest::SetUp();
        if (HasFatalFailure()) {
            return;
        }
        if (!git.exists()) {
            GTEST_SKIP() << "git is not available";
        }
        ASSERT_TRUE(git.exec({ "init", "--initial-branch=main", repository.string() }));
        ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }));
        ASSERT_TRUE(
          git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }));
        auto committed = commitSource("initial content");
        ASSERT_TRUE(committed) << committed.error().message();
        initialCommit = *committed;
        source.kind = "git";
        source.url = repository.string();
        source.commit = "main";
        source.name = "source";
        source.submodules = false;
    }

    auto gitHead(const std::filesystem::path &checkout) -> utils::error::Result<std::string>
    {
        LINGLONG_TRACE("read test git HEAD");
        auto head = git.exec({ "-C", checkout.string(), "rev-parse", "HEAD" });
        if (!head) {
            return LINGLONG_ERR(head);
        }
        return head->substr(0, head->find_first_of("\r\n"));
    }

    auto commitSource(const std::string &contents) -> utils::error::Result<std::string>
    {
        LINGLONG_TRACE("commit test git source");
        std::ofstream(repository / "source.txt") << contents;
        auto added = git.exec({ "-C", repository.string(), "add", "source.txt" });
        if (!added) {
            return LINGLONG_ERR(added);
        }
        auto committed = git.exec({ "-C", repository.string(), "commit", "-m", contents });
        if (!committed) {
            return LINGLONG_ERR(committed);
        }
        return gitHead(repository);
    }

    utils::Cmd git{ "git" };
    const std::filesystem::path repository = temporary.path() / "repository";
    api::types::v1::BuilderProjectSource source;
    std::string initialCommit;
};

TEST_F(ProjectLockGitTest, BranchAdvanceKeepsLockedHashUntilExplicitUpdate)
{
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    ASSERT_EQ(lock->sources.front().resolved.hash, "git-commit:" + initialCommit);
    const auto serialized = nlohmann::json(*lock);
    EXPECT_EQ(serialized["sources"][0]["source"]["commit"], "main");
    EXPECT_FALSE(serialized["sources"][0]["resolved"].contains("commit"));

    auto advanced = commitSource("new content");
    ASSERT_TRUE(advanced) << advanced.error().message();
    ASSERT_NE(*advanced, initialCommit);
    const std::vector<std::filesystem::path> caches{ cache, temporary.path() / "empty-cache" };
    for (std::size_t index = 0; index < caches.size(); ++index) {
        SCOPED_TRACE(index);
        const auto destination = temporary.path() / ("normal-" + std::to_string(index));
        auto resolved =
          resolveAndFetchProjectSources({ source }, caches[index], destination, false, *lock);
        ASSERT_TRUE(resolved) << resolved.error().message();
        EXPECT_FALSE(resolved->changed);
        EXPECT_EQ(resolved->entries.front().resolved.hash, "git-commit:" + initialCommit);
        auto head = gitHead(destination / *source.name);
        ASSERT_TRUE(head) << head.error().message();
        EXPECT_EQ(*head, initialCommit);
    }
    const auto lockedDestination = temporary.path() / "locked";
    auto verified = verifyAndFetchProjectSources({ source }, cache, lockedDestination, true, *lock);
    ASSERT_TRUE(verified) << verified.error().message();
    auto lockedHead = gitHead(lockedDestination / *source.name);
    ASSERT_TRUE(lockedHead) << lockedHead.error().message();
    EXPECT_EQ(*lockedHead, initialCommit);

    auto updated =
      prepareSourcesFromManifest({ source }, manifest, cache, SourcePreparationMode::Update);
    ASSERT_TRUE(updated) << updated.error().message();
    EXPECT_TRUE(updated->changed);
    EXPECT_EQ(updated->entries.front().resolved.hash, "git-commit:" + *advanced);
    ASSERT_TRUE(
      writeCompleteProjectLock(manifest,
                               completeProjectLock(updated->entries, lock->dependencies.front())));
    auto reused =
      prepareSourcesFromManifest({ source }, manifest, cache, SourcePreparationMode::UseOrGenerate);
    ASSERT_TRUE(reused) << reused.error().message();
    EXPECT_FALSE(reused->changed);
    EXPECT_EQ(reused->entries.front().resolved.hash, "git-commit:" + *advanced);
}

TEST_F(ProjectLockGitTest, RejectsDuplicateRequestsAfterBranchAdvances)
{
    source.name = "A";
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    const auto previous = nlohmann::json(*lock);

    auto advanced = commitSource("new content");
    ASSERT_TRUE(advanced) << advanced.error().message();
    ASSERT_NE(*advanced, initialCommit);
    auto duplicate = source;
    duplicate.name = "B";
    duplicate.version = "different description";

    for (const auto mode : { SourcePreparationMode::UseOrGenerate,
                             SourcePreparationMode::Locked,
                             SourcePreparationMode::Generate,
                             SourcePreparationMode::Update }) {
        SCOPED_TRACE(static_cast<int>(mode));
        auto result = prepareSourcesFromManifest({ source, duplicate }, manifest, cache, mode);
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().message().find("duplicate source fetch request: A and B"),
                  std::string::npos)
          << result.error().message();
    }

    SourceCacheLayout layout(cache);
    EXPECT_FALSE(std::filesystem::exists(layout.gitObject(*advanced, false)));
    auto preserved = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(preserved) << preserved.error().message();
    EXPECT_EQ(nlohmann::json(*preserved), previous);
}

TEST_F(ProjectLockGitTest, ChangedBranchNameResolvesNewCommitInNormalBuild)
{
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    auto advanced = commitSource("new content");
    ASSERT_TRUE(advanced) << advanced.error().message();
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "branch", "other" }));
    auto changed = source;
    changed.commit = "other";
    auto verified =
      verifyAndFetchProjectSources({ changed }, cache, temporary.path() / "locked", false, *lock);
    ASSERT_FALSE(verified);
    EXPECT_NE(verified.error().message().find("sources differ from linglong.lock"),
              std::string::npos);

    const auto destination = temporary.path() / "changed";
    auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, false, *lock);
    ASSERT_TRUE(resolved) << resolved.error().message();
    EXPECT_TRUE(resolved->changed);
    EXPECT_EQ(resolved->entries.front().source.commit, changed.commit);
    EXPECT_EQ(resolved->entries.front().resolved.hash, "git-commit:" + *advanced);
    auto head = gitHead(destination / *changed.name);
    ASSERT_TRUE(head) << head.error().message();
    EXPECT_EQ(*head, *advanced);
}

TEST_F(ProjectLockGitTest, ReadsLegacyResolvedCommitAndWritesOnlyHash)
{
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    auto legacy = nlohmann::json(*lock);
    legacy["sources"][0]["resolved"]["commit"] = initialCommit;
    // JSON is valid YAML, so this exercises the project lock file reader as well.
    std::ofstream(projectLockPath(manifest)) << legacy.dump(2) << '\n';
    auto loaded = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(loaded) << loaded.error().message();
    EXPECT_EQ(loaded->sources.front().resolved.hash, "git-commit:" + initialCommit);
    auto verified =
      verifyAndFetchProjectSources({ source }, cache, temporary.path() / "locked", true, *loaded);
    ASSERT_TRUE(verified) << verified.error().message();
    ASSERT_TRUE(writeCompleteProjectLock(manifest, *loaded));
    const auto written = YAML::LoadFile(projectLockPath(manifest).string());
    EXPECT_EQ(written["sources"][0]["source"]["commit"].as<std::string>(), "main");
    EXPECT_FALSE(written["sources"][0]["resolved"]["commit"].IsDefined());
}

TEST_F(ProjectLockGitTest, InvalidHashDoesNotFallBackToDeclaredBranch)
{
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    lock->sources.front().resolved.hash = "git-commit:main";
    auto verified =
      verifyAndFetchProjectSources({ source }, cache, temporary.path() / "locked", false, *lock);
    ASSERT_FALSE(verified);
    EXPECT_NE(verified.error().message().find("git source has an invalid hash"), std::string::npos);

    SourceFetcher fetcher(LockedSourceRequest{ source, lock->sources.front().resolved },
                          SourceCacheLayout(cache));
    const auto destination = temporary.path() / "fetched";
    std::filesystem::create_directories(destination);
    auto fetched = fetcher.fetch(destination);
    ASSERT_FALSE(fetched);
    EXPECT_NE(fetched.error().message().find("invalid git source hash"), std::string::npos);
}

TEST(ProjectLock, DerivesLockPathFromManifest)
{
    EXPECT_EQ(projectLockPath("/project/linglong.yaml"), "/project/linglong.lock");
    EXPECT_EQ(projectLockPath("/project/linglong.x86_64.yaml"), "/project/linglong.x86_64.lock");
}

TEST(SourceCacheLayout, DefinesObjectRootsAndCleansTemporaryDirectories)
{
    TempDir temporary;
    SourceCacheLayout cache(temporary.path() / "cache");

    EXPECT_EQ(cache.root(), temporary.path() / "cache/sources/v1");
    EXPECT_EQ(cache.object("file", "sha256:abc"), cache.root() / "file/sha256/abc");
    EXPECT_EQ(cache.object("archive", "sha256:abc"), cache.root() / "archive/sha256/abc");
    EXPECT_EQ(cache.object("dsc", "sha256:abc"), cache.root() / "dsc/sha256/abc");
    EXPECT_EQ(cache.gitObject("ABCDEF", true), cache.root() / "git/commit/abcdef-submodules");
    EXPECT_EQ(cache.gitObject("ABCDEF", false), cache.root() / "git/commit/abcdef-no-submodules");
    EXPECT_EQ(cache.temporary(), cache.root() / "tmp");
    EXPECT_EQ(cache.lockFile(cache.object("file", "sha256:abc")),
              cache.root() / "locks/file/sha256/abc.lock");
    EXPECT_EQ(cache.lockFile(cache.gitObject("ABCDEF", true)),
              cache.root() / "locks/git/commit/abcdef-submodules.lock");
    EXPECT_NE(cache.lockFile(cache.gitObject("ABCDEF", true)),
              cache.lockFile(cache.gitObject("ABCDEF", false)));

    std::filesystem::path temporaryPath;
    {
        ASSERT_TRUE(cache.ensure());
        auto directory = utils::TemporaryDirectory::create("test-", cache.temporary());
        ASSERT_TRUE(directory.has_value()) << directory.error().message();
        temporaryPath = directory->path();
        EXPECT_EQ(temporaryPath.parent_path(), cache.temporary());
        std::ofstream(temporaryPath / "file") << "temporary";
    }
    EXPECT_FALSE(std::filesystem::exists(temporaryPath));
    EXPECT_TRUE(std::filesystem::exists(cache.temporary()));
}

TEST(ProjectLock, ReadRequiresExistingFile)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";

    auto result = readProjectLock(projectLockPath(manifest));
    EXPECT_FALSE(result.has_value());
}

TEST(ProjectLock, ReadRejectsUnsupportedFormatVersion)
{
    TempDir temporary;
    const auto path = temporary.path() / "linglong.lock";
    std::ofstream(path) << "version: 2\nsources: []\ndependencies: []\n";

    auto result = readProjectLock(path);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("unsupported project lock version"), std::string::npos);
}

TEST(ProjectLock, UnspecifiedArchitectureUsesBuildHost)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    const LockedDependency base{ "org.test.Base/1.0",
                                 { { "binary" }, "main:org.test.Base/1.0.0/unknown" },
                                 "base" };
    const auto lock = completeProjectLock({}, base);
    ASSERT_TRUE(writeCompleteProjectLock(manifest, lock));
    auto loaded = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(loaded) << loaded.error().message();
    const auto serialized = nlohmann::json(*loaded);
    EXPECT_FALSE(serialized.contains("build_architecture"));
    EXPECT_EQ(loaded->dependencies.front().resolved.reference, base.resolved.reference);

    auto reference = lockedDependencyReference(loaded->dependencies.front());
    ASSERT_TRUE(reference) << reference.error().message();
    EXPECT_EQ(reference->arch, package::Architecture::currentCPUArchitecture());
    EXPECT_EQ(reference->version.toString(), "1.0.0");
    EXPECT_EQ(nlohmann::json(*loaded), serialized);
}

TEST(ProjectLock, LockedDependencyPreservesExplicitArchitecture)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    const LockedDependency base{ "org.test.Base/1.0/arm64",
                                 { { "binary" }, "main:org.test.Base/1.0.0/arm64" },
                                 "base" };
    ASSERT_TRUE(writeCompleteProjectLock(manifest, completeProjectLock({}, base)));
    auto loaded = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(loaded) << loaded.error().message();
    auto reference = lockedDependencyReference(loaded->dependencies.front());
    ASSERT_TRUE(reference) << reference.error().message();
    EXPECT_EQ(reference->toString(), base.resolved.reference);
}

TEST(ProjectLock, LockedDependencyRequiresAnExactReference)
{
    for (const auto &reference : { "org.test.Base/1.0.0",
                                   "main:org.test.Base",
                                   "main:org.test.Base/unknown",
                                   "main:org.test.Base/1.0.0/unkown" }) {
        SCOPED_TRACE(reference);
        const LockedDependency base{ "org.test.Base", { { "binary" }, reference }, "base" };
        EXPECT_FALSE(lockedDependencyReference(base));
    }
}

TEST(ProjectLock, FetchEmptySourcePlanPreparesDirectories)
{
    for (const bool offline : { false, true }) {
        SCOPED_TRACE(offline);
        TempDir temporary;
        const auto cache = temporary.path() / "cache";
        const auto destination = temporary.path() / "sources";
        int progressCalls = 0;
        const auto progress =
          [&](std::size_t, const api::types::v1::BuilderProjectSource &, SourceFetchStatus) {
              ++progressCalls;
          };
        auto entries = fetchProjectSources({}, cache, destination, offline, progress);
        ASSERT_TRUE(entries) << entries.error().message();
        EXPECT_TRUE(entries->empty());
        EXPECT_EQ(progressCalls, 0);
        EXPECT_TRUE(std::filesystem::is_directory(SourceCacheLayout(cache).temporary()));
        EXPECT_TRUE(std::filesystem::is_directory(destination));
    }
}

TEST(ProjectLock, FetchInvalidSourceRequestReportsError)
{
    TempDir temporary;
    const auto cache = temporary.path() / "cache";
    const auto destination = temporary.path() / "sources";
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.name = "source";
    ProjectSourcePlan plan;
    plan.requests.emplace_back(GenerateSourceRequest{ source });
    auto entries = fetchProjectSources(plan, cache, destination, false);
    ASSERT_FALSE(entries);
    EXPECT_NE(entries.error().message().find("source URL is missing"), std::string::npos);
    EXPECT_TRUE(std::filesystem::is_directory(SourceCacheLayout(cache).temporary()));
    EXPECT_TRUE(std::filesystem::is_directory(destination));
}

TEST(ProjectLock, GeneratesAndUpdatesEmptyLock)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";

    auto generated = prepareSourcesFromManifest({},
                                                manifest,
                                                temporary.path() / "cache",
                                                SourcePreparationMode::Generate);
    ASSERT_TRUE(generated.has_value()) << generated.error().message();
    EXPECT_TRUE(generated->changed);
    EXPECT_FALSE(std::filesystem::exists(projectLockPath(manifest)));
    const LockedDependency base{ "org.test.Base",
                                 { { "binary", "develop" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    ASSERT_TRUE(writeCompleteProjectLock(manifest, completeProjectLock(generated->entries, base)));
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::exists(projectLockPath(manifest), ec));
    EXPECT_FALSE(ec);
    auto lock =
      utils::serialize::LoadYAMLFile<api::types::v1::BuilderProjectLock>(projectLockPath(manifest));
    ASSERT_TRUE(lock.has_value()) << lock.error().message();
    EXPECT_EQ(lock->version, 1);
    EXPECT_TRUE(lock->sources.empty());

    auto updated = prepareSourcesFromManifest({},
                                              manifest,
                                              temporary.path() / "cache",
                                              SourcePreparationMode::Update);
    EXPECT_TRUE(updated.has_value()) << updated.error().message();
    EXPECT_TRUE(updated->changed);
    api::types::v1::BuilderProjectLock incomplete;
    incomplete.version = 1;
    incomplete.sources = updated->entries;
    EXPECT_FALSE(writeCompleteProjectLock(manifest, incomplete).has_value());
    EXPECT_TRUE(std::filesystem::exists(projectLockPath(manifest)));

    EXPECT_FALSE(std::filesystem::exists(temporary.path() / "cache/sources/v1/.lock", ec));
    EXPECT_FALSE(ec);
}

TEST(ProjectLock, RecordsCompleteDependenciesAndKeepsExistingLockReadOnly)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    auto generated = prepareSourcesFromManifest({},
                                                manifest,
                                                temporary.path() / "cache",
                                                SourcePreparationMode::Generate);
    ASSERT_TRUE(generated.has_value()) << generated.error().message();
    const LockedDependency base{ "org.test.Base",
                                 { { "binary", "develop", "locale" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    const LockedDependency runtime{ "org.test.Runtime",
                                    { { "binary", "develop" }, "main:org.test.Runtime/2.0.0" },
                                    "runtime" };
    auto complete = completeProjectLock(generated->entries, base, runtime);
    ASSERT_TRUE(writeCompleteProjectLock(manifest, complete));

    auto lock =
      utils::serialize::LoadYAMLFile<api::types::v1::BuilderProjectLock>(projectLockPath(manifest));
    ASSERT_TRUE(lock.has_value()) << lock.error().message();
    ASSERT_EQ(lock->dependencies.size(), 2);
    EXPECT_EQ(lock->dependencies[0].type, "base");
    EXPECT_EQ(lock->dependencies[0].resolved.modules,
              (std::vector<std::string>{ "binary", "develop", "locale" }));
    EXPECT_EQ(lock->dependencies[1].type, "runtime");
    auto state = projectLockDependencyState(*lock);
    ASSERT_TRUE(state.has_value()) << state.error().message();
    auto preparedState = projectLockDependencyState(complete);
    ASSERT_TRUE(preparedState.has_value()) << preparedState.error().message();
    EXPECT_EQ(*state, *preparedState);
    const auto stateJson = nlohmann::json::parse(*state);
    ASSERT_TRUE(stateJson.contains("base"));
    ASSERT_TRUE(stateJson.contains("runtime"));
    EXPECT_EQ(stateJson.at("base").at("reference"),
              "main:org.test.Base/1.0.0/"
                + package::Architecture::currentCPUArchitecture().toString());
    EXPECT_EQ(stateJson.at("base").at("modules").at(2), "locale");
    EXPECT_FALSE(stateJson.at("base").contains("requested"));
    EXPECT_FALSE(stateJson.at("base").contains("commit"));

    auto locked = prepareSourcesFromManifest({},
                                             manifest,
                                             temporary.path() / "cache",
                                             SourcePreparationMode::Locked);
    ASSERT_TRUE(locked.has_value()) << locked.error().message();
    EXPECT_FALSE(locked->changed);
    EXPECT_TRUE(locked->entries.empty());

    auto updated = prepareSourcesFromManifest({},
                                              manifest,
                                              temporary.path() / "cache",
                                              SourcePreparationMode::Update);
    ASSERT_TRUE(updated.has_value()) << updated.error().message();
    EXPECT_TRUE(updated->changed);
    auto loaded = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
    auto unchangedState = projectLockDependencyState(*loaded);
    ASSERT_TRUE(unchangedState);
    EXPECT_EQ(*state, *unchangedState);

    ASSERT_TRUE(std::filesystem::remove(projectLockPath(manifest)));
    auto reused = verifyAndFetchProjectSources({},
                                               temporary.path() / "cache",
                                               temporary.path() / "sources",
                                               false,
                                               *loaded);
    ASSERT_TRUE(reused.has_value()) << reused.error().message();
}

TEST(ProjectLock, MissingAndEmptySourcesHaveTheSamePlanningSemantics)
{
    const std::vector<std::optional<std::vector<api::types::v1::BuilderProjectSource>>>
      declarations{
          std::nullopt,
          std::vector<api::types::v1::BuilderProjectSource>{},
      };
    api::types::v1::BuilderProjectLock emptyLock{};
    api::types::v1::BuilderProjectLock populatedLock{};
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/source";
    populatedLock.sources = { { { "sha256:" + std::string(64, 'a') }, source } };

    for (const auto &sources : declarations) {
        SCOPED_TRACE(sources.has_value());
        auto fresh = resolveProjectLockSources(sources, std::nullopt);
        ASSERT_TRUE(fresh) << fresh.error().message();
        EXPECT_TRUE(fresh->changed);
        EXPECT_TRUE(fresh->requests.empty());

        auto unchanged = resolveProjectLockSources(sources, emptyLock);
        ASSERT_TRUE(unchanged) << unchanged.error().message();
        EXPECT_FALSE(unchanged->changed);
        EXPECT_TRUE(unchanged->requests.empty());

        auto removed = resolveProjectLockSources(sources, populatedLock);
        ASSERT_TRUE(removed) << removed.error().message();
        EXPECT_TRUE(removed->changed);
        EXPECT_TRUE(removed->requests.empty());

        auto verified = verifyProjectLockSources(sources, emptyLock);
        ASSERT_TRUE(verified) << verified.error().message();
        EXPECT_FALSE(verified->changed);
        EXPECT_TRUE(verified->requests.empty());

        auto mismatch = verifyProjectLockSources(sources, populatedLock);
        ASSERT_FALSE(mismatch);
        EXPECT_NE(mismatch.error().message().find("sources differ from linglong.lock"),
                  std::string::npos);
        EXPECT_EQ(populatedLock.sources.size(), 1);
    }
}

TEST_F(ProjectLockSourceTest, PlansLockedAndFreshSourcesWithoutFetching)
{
    auto first = fileSource("first", "locked content");
    auto second = fileSource("second", "new content");
    api::types::v1::BuilderProjectLock lock{};
    lock.sources = { { { *first.digest }, first } };
    ASSERT_TRUE(std::filesystem::remove(*first.url));
    ASSERT_TRUE(std::filesystem::remove(*second.url));

    auto plan = resolveProjectLockSources(std::vector{ first, second }, lock);
    ASSERT_TRUE(plan) << plan.error().message();
    EXPECT_TRUE(plan->changed);
    ASSERT_EQ(plan->requests.size(), 2);
    ASSERT_TRUE(std::holds_alternative<LockedSourceRequest>(plan->requests[0]));
    EXPECT_EQ(std::get<LockedSourceRequest>(plan->requests[0]).resolved.hash, *first.digest);
    ASSERT_TRUE(std::holds_alternative<GenerateSourceRequest>(plan->requests[1]));
    EXPECT_EQ(std::get<GenerateSourceRequest>(plan->requests[1]).source.url, second.url);

    auto verified = verifyProjectLockSources(std::vector{ first }, lock);
    ASSERT_TRUE(verified) << verified.error().message();
    EXPECT_FALSE(verified->changed);
    ASSERT_EQ(verified->requests.size(), 1);
    EXPECT_TRUE(std::holds_alternative<LockedSourceRequest>(verified->requests.front()));
    EXPECT_TRUE(downloadRequests().empty());
    EXPECT_FALSE(std::filesystem::exists(cache));
}

TEST_F(ProjectLockSourceTest, FetchStartsOnlyAfterExplicitCall)
{
    auto source = fileSource("source", "source content");
    const auto destination = temporary.path() / "sources";
    auto plan = resolveProjectLockSources(std::vector{ source }, std::nullopt);
    ASSERT_TRUE(plan) << plan.error().message();
    EXPECT_TRUE(downloadRequests().empty());
    EXPECT_FALSE(std::filesystem::exists(cache));
    EXPECT_FALSE(std::filesystem::exists(destination));

    auto entries = fetchProjectSources(*plan, cache, destination, false);
    ASSERT_TRUE(entries) << entries.error().message();
    ASSERT_EQ(entries->size(), 1);
    EXPECT_EQ(entries->front().resolved.hash, *source.digest);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url }));
    auto outputHash = fileHash(destination / *source.name);
    ASSERT_TRUE(outputHash) << outputHash.error().message();
    EXPECT_EQ(*outputHash, *source.digest);
    EXPECT_TRUE(std::filesystem::is_directory(SourceCacheLayout(cache).temporary()));
}

TEST_F(ProjectLockSourceTest, ReportsProgressForFreshAndCachedSourcesInYamlOrder)
{
    const auto first = fileSource("first", "first content");
    const auto second = fileSource("second", "second content");
    const std::vector<api::types::v1::BuilderProjectSource> sources{ second, first };
    auto plan = resolveProjectLockSources(sources, std::nullopt);
    ASSERT_TRUE(plan) << plan.error().message();
    using Event = std::pair<std::size_t, SourceFetchStatus>;
    std::vector<Event> events;
    const auto progress = [&](std::size_t index,
                              const api::types::v1::BuilderProjectSource &source,
                              SourceFetchStatus status) {
        ASSERT_LT(index, sources.size());
        EXPECT_EQ(source.url, sources[index].url);
        EXPECT_EQ(source.name, sources[index].name);
        events.emplace_back(index, status);
    };

    auto entries = fetchProjectSources(*plan, cache, temporary.path() / "fresh", false, progress);
    ASSERT_TRUE(entries) << entries.error().message();
    const std::vector<Event> expected{ { 0, SourceFetchStatus::Fetching },
                                       { 0, SourceFetchStatus::Complete },
                                       { 1, SourceFetchStatus::Fetching },
                                       { 1, SourceFetchStatus::Complete } };
    EXPECT_EQ(events, expected);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *second.url, *first.url }));

    api::types::v1::BuilderProjectLock lock{};
    lock.sources = *entries;
    auto lockedPlan = verifyProjectLockSources(sources, lock);
    ASSERT_TRUE(lockedPlan) << lockedPlan.error().message();
    ASSERT_TRUE(std::filesystem::remove(*first.url));
    ASSERT_TRUE(std::filesystem::remove(*second.url));
    events.clear();
    auto cached =
      fetchProjectSources(*lockedPlan, cache, temporary.path() / "cached", true, progress);
    ASSERT_TRUE(cached) << cached.error().message();
    EXPECT_EQ(events, expected);
    EXPECT_EQ(downloadRequests().size(), 2);
    EXPECT_TRUE(std::filesystem::exists(temporary.path() / "cached" / *first.name));
    EXPECT_TRUE(std::filesystem::exists(temporary.path() / "cached" / *second.name));
}

TEST_F(ProjectLockSourceTest, ReportsFailureAndStopsBeforeLaterSources)
{
    const auto first = fileSource("first", "first content");
    auto second = fileSource("second", "second content");
    second.digest = "sha256:" + std::string(64, '0');
    const auto third = fileSource("third", "third content");
    auto plan = resolveProjectLockSources(std::vector{ first, second, third }, std::nullopt);
    ASSERT_TRUE(plan) << plan.error().message();
    using Event = std::pair<std::size_t, SourceFetchStatus>;
    std::vector<Event> events;
    auto fetched = fetchProjectSources(*plan,
                                       cache,
                                       temporary.path() / "sources",
                                       false,
                                       [&](std::size_t index,
                                           const api::types::v1::BuilderProjectSource &,
                                           SourceFetchStatus status) {
                                           events.emplace_back(index, status);
                                       });

    ASSERT_FALSE(fetched);
    EXPECT_EQ(events,
              (std::vector<Event>{ { 0, SourceFetchStatus::Fetching },
                                   { 0, SourceFetchStatus::Complete },
                                   { 1, SourceFetchStatus::Fetching },
                                   { 1, SourceFetchStatus::Failed } }));
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *first.url, *second.url }));
    EXPECT_TRUE(std::filesystem::exists(temporary.path() / "sources" / *first.name));
    EXPECT_FALSE(std::filesystem::exists(temporary.path() / "sources" / *second.name));
    EXPECT_FALSE(std::filesystem::exists(temporary.path() / "sources" / *third.name));
}

TEST_F(ProjectLockSourceTest, PlanUsesYamlOrderAndNamesWithoutChangingLock)
{
    auto first = fileSource("first", "first content");
    auto second = fileSource("second", "second content");
    api::types::v1::BuilderProjectLock lock{};
    lock.version = 1;
    lock.sources = { { { *second.digest }, second }, { { *first.digest }, first } };
    const auto previous = nlohmann::json(lock);
    first.name = "renamed";
    first.version = "new version";

    auto plan = resolveProjectLockSources(std::vector{ first, second }, lock);
    ASSERT_TRUE(plan) << plan.error().message();
    EXPECT_FALSE(plan->changed);
    ASSERT_EQ(plan->requests.size(), 2);
    ASSERT_TRUE(std::holds_alternative<LockedSourceRequest>(plan->requests[0]));
    const auto &request = std::get<LockedSourceRequest>(plan->requests[0]);
    EXPECT_EQ(request.source.name, first.name);
    EXPECT_EQ(request.source.version, first.version);
    EXPECT_EQ(request.resolved.hash, *first.digest);
    auto verified = verifyProjectLockSources(std::vector{ first, second }, lock);
    ASSERT_TRUE(verified) << verified.error().message();
    EXPECT_FALSE(verified->changed);
    EXPECT_EQ(nlohmann::json(lock), previous);
    EXPECT_TRUE(downloadRequests().empty());
    EXPECT_FALSE(std::filesystem::exists(cache));
}

TEST_F(ProjectLockSourceTest, RemovedSourcesMarkPlanChanged)
{
    auto first = fileSource("first", "first content");
    auto second = fileSource("second", "second content");
    api::types::v1::BuilderProjectLock lock{};
    lock.sources = { { { *first.digest }, first }, { { *second.digest }, second } };

    auto plan = resolveProjectLockSources(std::vector{ first }, lock);
    ASSERT_TRUE(plan) << plan.error().message();
    EXPECT_TRUE(plan->changed);
    ASSERT_EQ(plan->requests.size(), 1);
    EXPECT_TRUE(std::holds_alternative<LockedSourceRequest>(plan->requests.front()));
    auto removedAll = resolveProjectLockSources(std::nullopt, lock);
    ASSERT_TRUE(removedAll) << removedAll.error().message();
    EXPECT_TRUE(removedAll->changed);
    EXPECT_TRUE(removedAll->requests.empty());
    auto verified = verifyProjectLockSources(std::vector{ first }, lock);
    ASSERT_FALSE(verified);
    EXPECT_NE(verified.error().message().find("sources differ from linglong.lock"),
              std::string::npos);
    EXPECT_TRUE(downloadRequests().empty());
    EXPECT_FALSE(std::filesystem::exists(cache));
}

TEST_F(ProjectLockSourceTest, NormalBuildUpdatesOnlyChangedSource)
{
    auto first = fileSource("first", "first version");
    auto second = fileSource("second", "second version");
    auto generated = generateLock({ first, second });
    ASSERT_TRUE(generated) << generated.error().message();
    const auto originalSecond = generated->sources[1].resolved.hash;
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *first.url, *second.url }));

    first = fileSource("first", "new first version");
    auto changed = prepareSourcesFromManifest({ first, second },
                                              manifest,
                                              cache,
                                              SourcePreparationMode::UseOrGenerate);
    ASSERT_TRUE(changed) << changed.error().message();
    EXPECT_TRUE(changed->changed);
    EXPECT_EQ(changed->entries[0].resolved.hash, *first.digest);
    EXPECT_EQ(changed->entries[1].resolved.hash, originalSecond);
    ASSERT_TRUE(writeCompleteProjectLock(
      manifest,
      completeProjectLock(changed->entries, generated->dependencies.front())));
    EXPECT_EQ(downloadRequests(),
              (std::vector<std::string>{ *first.url, *second.url, *first.url }));

    auto unchanged = prepareSourcesFromManifest({ first, second },
                                                manifest,
                                                cache,
                                                SourcePreparationMode::UseOrGenerate);
    ASSERT_TRUE(unchanged) << unchanged.error().message();
    EXPECT_FALSE(unchanged->changed);

    auto locked = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(locked) << locked.error().message();
    auto verified = verifyAndFetchProjectSources({ first, second },
                                                 cache,
                                                 temporary.path() / "verified",
                                                 false,
                                                 *locked);
    ASSERT_TRUE(verified) << verified.error().message();
    EXPECT_EQ(downloadRequests().size(), 3);

    auto explicitUpdate =
      prepareSourcesFromManifest({ first, second }, manifest, cache, SourcePreparationMode::Update);
    ASSERT_TRUE(explicitUpdate) << explicitUpdate.error().message();
    EXPECT_TRUE(explicitUpdate->changed);
    EXPECT_EQ(
      downloadRequests(),
      (std::vector<std::string>{ *first.url, *second.url, *first.url, *first.url, *second.url }));
}

TEST_F(ProjectLockSourceTest, ChangedUrlDownloadsEvenWhenDigestIsCached)
{
    auto source = fileSource("original", "same content");
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();

    auto changed = fileSource("replacement", "same content");
    changed.name = source.name;
    ASSERT_EQ(changed.digest, source.digest);
    auto resolved = prepareSourcesFromManifest({ changed },
                                               manifest,
                                               cache,
                                               SourcePreparationMode::UseOrGenerate);
    ASSERT_TRUE(resolved) << resolved.error().message();
    EXPECT_TRUE(resolved->changed);
    ASSERT_EQ(resolved->entries.size(), 1);
    EXPECT_EQ(resolved->entries.front().source.url, changed.url);
    EXPECT_EQ(resolved->entries.front().resolved.hash, *source.digest);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url, *changed.url }));

    ASSERT_TRUE(
      writeCompleteProjectLock(manifest,
                               completeProjectLock(resolved->entries, lock->dependencies.front())));
    auto updated = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(updated) << updated.error().message();
    EXPECT_EQ(updated->sources.front().source.url, changed.url);
}

TEST_F(ProjectLockSourceTest, ChangedUrlWithStaleDigestFailsBeforePublishing)
{
    auto source = fileSource("original", "original content");
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    const auto previousLock = nlohmann::json(*lock).dump();

    auto changed = fileSource("replacement", "different content");
    ASSERT_NE(changed.digest, source.digest);
    const auto replacementDigest = *changed.digest;
    changed.digest = source.digest;
    changed.name = source.name;
    const auto destination = temporary.path() / "changed";
    auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, false, *lock);
    ASSERT_FALSE(resolved);
    EXPECT_NE(resolved.error().message().find("source hash does not match digest in linglong.yaml"),
              std::string::npos)
      << resolved.error().message();
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url, *changed.url }));
    EXPECT_FALSE(std::filesystem::exists(destination / *changed.name));
    SourceCacheLayout layout(cache);
    EXPECT_FALSE(std::filesystem::exists(layout.object("file", replacementDigest)));
    auto cachedHash = fileHash(layout.object("file", *source.digest));
    ASSERT_TRUE(cachedHash) << cachedHash.error().message();
    EXPECT_EQ(*cachedHash, *source.digest);
    auto preserved = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(preserved) << preserved.error().message();
    EXPECT_EQ(nlohmann::json(*preserved).dump(), previousLock);
}

TEST_F(ProjectLockSourceTest, ChangedUrlWithoutDigestResolvesNewContent)
{
    auto source = fileSource("original", "original content");
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();

    auto changed = fileSource("replacement", "different content");
    const auto replacementDigest = *changed.digest;
    changed.digest.reset();
    changed.name = source.name;
    const auto destination = temporary.path() / "changed";
    auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, false, *lock);
    ASSERT_TRUE(resolved) << resolved.error().message();
    EXPECT_TRUE(resolved->changed);
    ASSERT_EQ(resolved->entries.size(), 1);
    EXPECT_EQ(resolved->entries.front().source.url, changed.url);
    EXPECT_EQ(resolved->entries.front().resolved.hash, replacementDigest);
    auto outputHash = fileHash(destination / *changed.name);
    ASSERT_TRUE(outputHash) << outputHash.error().message();
    EXPECT_EQ(*outputHash, replacementDigest);
    SourceCacheLayout layout(cache);
    EXPECT_TRUE(std::filesystem::exists(layout.object("file", replacementDigest)));
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url, *changed.url }));
}

class ProjectLockDigestTest : public ProjectLockSourceTest,
                              public ::testing::WithParamInterface<std::string>
{
};

TEST_P(ProjectLockDigestTest, DigestChangesDownloadWithoutChangingUrl)
{
    auto source = fileSource("source", "original content");
    if (GetParam() == "Add") {
        source.digest.reset();
    }
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();

    auto changed = source;
    if (GetParam() == "Add") {
        changed.digest = lock->sources.front().resolved.hash;
    } else {
        changed = fileSource("source", "updated content");
        if (GetParam() == "Remove") {
            changed.digest.reset();
        }
    }
    ASSERT_EQ(changed.url, source.url);
    ASSERT_NE(changed.digest, source.digest);
    auto expectedHash = fileHash(*changed.url);
    ASSERT_TRUE(expectedHash) << expectedHash.error().message();
    SourceCacheLayout layout(cache);
    const auto cached = layout.object("file", *expectedHash);
    if (!std::filesystem::exists(cached)) {
        std::filesystem::copy_file(*changed.url, cached);
    }

    auto verified =
      verifyAndFetchProjectSources({ changed }, cache, temporary.path() / "locked", false, *lock);
    ASSERT_FALSE(verified);
    EXPECT_NE(verified.error().message().find("sources differ from linglong.lock"),
              std::string::npos);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url }));

    const auto destination = temporary.path() / "changed";
    auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, false, *lock);
    ASSERT_TRUE(resolved) << resolved.error().message();
    EXPECT_TRUE(resolved->changed);
    ASSERT_EQ(resolved->entries.size(), 1);
    EXPECT_EQ(resolved->entries.front().source.digest, changed.digest);
    EXPECT_EQ(resolved->entries.front().resolved.hash, *expectedHash);
    auto outputHash = fileHash(destination / *changed.name);
    ASSERT_TRUE(outputHash) << outputHash.error().message();
    EXPECT_EQ(*outputHash, *expectedHash);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url, *source.url }));

    ASSERT_TRUE(
      writeCompleteProjectLock(manifest,
                               completeProjectLock(resolved->entries, lock->dependencies.front())));
    auto reused = prepareSourcesFromManifest({ changed },
                                             manifest,
                                             cache,
                                             SourcePreparationMode::UseOrGenerate);
    ASSERT_TRUE(reused) << reused.error().message();
    EXPECT_FALSE(reused->changed);
    EXPECT_EQ(downloadRequests().size(), 2);
}

INSTANTIATE_TEST_SUITE_P(DigestChanges,
                         ProjectLockDigestTest,
                         ::testing::Values("Modify", "Add", "Remove"),
                         [](const ::testing::TestParamInfo<std::string> &info) {
                             return info.param;
                         });

TEST_F(ProjectLockSourceTest, ChangedDigestMustMatchDownloadedContent)
{
    auto source = fileSource("original", "original content");
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();

    auto other = fileSource("other", "different content");
    SourceCacheLayout layout(cache);
    std::filesystem::copy_file(*other.url, layout.object("file", *other.digest));
    auto changed = source;
    changed.digest = other.digest;
    ASSERT_NE(changed.digest, source.digest);
    const auto destination = temporary.path() / "changed";
    auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, false, *lock);
    ASSERT_FALSE(resolved);
    EXPECT_NE(resolved.error().message().find("source hash does not match digest in linglong.yaml"),
              std::string::npos)
      << resolved.error().message();
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url, *source.url }));
    EXPECT_FALSE(std::filesystem::exists(destination / *changed.name));
    auto preserved = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(preserved) << preserved.error().message();
    EXPECT_EQ(nlohmann::json(*preserved), nlohmann::json(*lock));
}

TEST_F(ProjectLockSourceTest, OfflineRequiresMatchingLockEntryEvenWhenDigestIsCached)
{
    auto source = fileSource("original", "same content");
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();

    auto changed = source;
    changed.url = (temporary.path() / "replacement").string();
    auto changedResult =
      resolveAndFetchProjectSources({ changed }, cache, temporary.path() / "changed", true, *lock);
    ASSERT_FALSE(changedResult);
    EXPECT_NE(changedResult.error().message().find("new or changed source in offline mode"),
              std::string::npos);

    auto generated = resolveAndFetchProjectSources({ source },
                                                   cache,
                                                   temporary.path() / "generated",
                                                   true,
                                                   std::nullopt);
    EXPECT_FALSE(generated);
    auto matched =
      resolveAndFetchProjectSources({ source }, cache, temporary.path() / "matched", true, *lock);
    ASSERT_TRUE(matched) << matched.error().message();
    EXPECT_FALSE(matched->changed);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url }));
}

TEST(ProjectLock, NonGitSubmodulesDoNotChangeSourceMatching)
{
    const std::vector<std::optional<bool>> values{ std::nullopt, false, true };
    for (const auto &kind : { "file", "archive", "dsc" }) {
        for (const auto &previous : values) {
            api::types::v1::BuilderProjectSource source;
            source.kind = kind;
            source.url = "https://example.com/source";
            source.submodules = previous;
            api::types::v1::BuilderProjectLock lock{};
            lock.sources = { { { "sha256:" + std::string(64, '0') }, source } };
            const auto originalLock = nlohmann::json(lock);

            for (const auto &current : values) {
                source.submodules = current;
                SCOPED_TRACE(originalLock.dump());
                SCOPED_TRACE(nlohmann::json(source).dump());
                auto plan = resolveProjectLockSources(std::vector{ source }, lock);
                ASSERT_TRUE(plan) << plan.error().message();
                EXPECT_FALSE(plan->changed);
                ASSERT_EQ(plan->requests.size(), 1);
                EXPECT_TRUE(std::holds_alternative<LockedSourceRequest>(plan->requests.front()));

                auto verified = verifyProjectLockSources(std::vector{ source }, lock);
                ASSERT_TRUE(verified) << verified.error().message();
                EXPECT_FALSE(verified->changed);
                ASSERT_EQ(verified->requests.size(), 1);
                EXPECT_TRUE(
                  std::holds_alternative<LockedSourceRequest>(verified->requests.front()));
                EXPECT_EQ(nlohmann::json(lock), originalLock);
            }
        }
    }
}

TEST(ProjectLock, NonGitCommitDoesNotChangeSourceMatching)
{
    const std::vector<std::optional<std::string>> values{ std::nullopt, "main", "release" };
    for (const auto &kind : { "file", "archive", "dsc" }) {
        for (const auto &previous : values) {
            api::types::v1::BuilderProjectSource source;
            source.kind = kind;
            source.url = "https://example.com/source";
            source.commit = previous;
            api::types::v1::BuilderProjectLock lock{};
            lock.sources = { { { "sha256:" + std::string(64, '0') }, source } };
            const auto originalLock = nlohmann::json(lock);

            for (const auto &current : values) {
                source.commit = current;
                SCOPED_TRACE(originalLock.dump());
                SCOPED_TRACE(nlohmann::json(source).dump());
                auto plan = resolveProjectLockSources(std::vector{ source }, lock);
                ASSERT_TRUE(plan) << plan.error().message();
                EXPECT_FALSE(plan->changed);
                ASSERT_EQ(plan->requests.size(), 1);
                EXPECT_TRUE(std::holds_alternative<LockedSourceRequest>(plan->requests.front()));

                auto verified = verifyProjectLockSources(std::vector{ source }, lock);
                ASSERT_TRUE(verified) << verified.error().message();
                EXPECT_FALSE(verified->changed);
                EXPECT_EQ(nlohmann::json(lock), originalLock);
            }
        }
    }
}

TEST(ProjectLock, GitCommitRemainsPartOfSourceMatching)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/source.git";
    source.commit = "main";
    api::types::v1::BuilderProjectLock lock{};
    lock.sources = { { { "git-commit:" + std::string(40, 'a') }, source } };
    source.commit = "release";

    auto plan = resolveProjectLockSources(std::vector{ source }, lock);
    ASSERT_TRUE(plan) << plan.error().message();
    EXPECT_TRUE(plan->changed);
    ASSERT_EQ(plan->requests.size(), 1);
    EXPECT_TRUE(std::holds_alternative<GenerateSourceRequest>(plan->requests.front()));
    EXPECT_FALSE(verifyProjectLockSources(std::vector{ source }, lock));
}

TEST(ProjectLock, GitSubmodulesRemainPartOfSourceMatching)
{
    const std::vector<std::optional<bool>> values{ std::nullopt, false, true };
    for (const auto &previous : values) {
        api::types::v1::BuilderProjectSource source;
        source.kind = "git";
        source.url = "https://example.com/source.git";
        source.commit = "main";
        source.submodules = previous;
        api::types::v1::BuilderProjectLock lock{};
        lock.sources = { { { "git-commit:" + std::string(40, 'a') }, source } };

        for (const auto &current : values) {
            source.submodules = current;
            const bool matches = previous.value_or(true) == current.value_or(true);
            auto plan = resolveProjectLockSources(std::vector{ source }, lock);
            ASSERT_TRUE(plan) << plan.error().message();
            EXPECT_EQ(plan->changed, !matches);
            ASSERT_EQ(plan->requests.size(), 1);
            EXPECT_EQ(std::holds_alternative<LockedSourceRequest>(plan->requests.front()), matches);

            auto verified = verifyProjectLockSources(std::vector{ source }, lock);
            EXPECT_EQ(verified.has_value(), matches);
        }
    }
}

TEST_F(ProjectLockSourceTest, NonGitCommitAndSubmodulesChangesReuseCachedContentOffline)
{
    auto source = fileSource("source", "locked content");
    source.commit = "main";
    source.submodules = true;
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    ASSERT_EQ(lock->sources.size(), 1);
    EXPECT_FALSE(lock->sources.front().source.commit.has_value());
    EXPECT_FALSE(lock->sources.front().source.submodules.has_value());
    ASSERT_TRUE(std::filesystem::remove(*source.url));
    source.commit = "release";

    for (const auto &value : std::vector<std::optional<bool>>{ std::nullopt, false, true }) {
        source.submodules = value;
        auto fetched = resolveAndFetchProjectSources({ source },
                                                     cache,
                                                     temporary.path() / "normal",
                                                     true,
                                                     *lock);
        ASSERT_TRUE(fetched) << fetched.error().message();
        EXPECT_FALSE(fetched->changed);
        auto verified =
          verifyAndFetchProjectSources({ source }, cache, temporary.path() / "locked", true, *lock);
        ASSERT_TRUE(verified) << verified.error().message();
    }

    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url }));
}

TEST_F(ProjectLockSourceTest, NameAndVersionChangesReuseLockedContent)
{
    auto source = fileSource("original", "locked content");
    source.version = "1";
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    ASSERT_TRUE(std::filesystem::remove(*source.url));
    const auto previousLock = nlohmann::json(*lock).dump();

    auto renamed = source;
    renamed.name = "renamed";
    auto nameRemoved = source;
    nameRemoved.name.reset();
    auto reversioned = source;
    reversioned.version = "2";
    auto bothChanged = renamed;
    bothChanged.version.reset();
    std::vector<api::types::v1::BuilderProjectSource> changes{ renamed,
                                                               nameRemoved,
                                                               reversioned,
                                                               bothChanged };
    for (std::size_t index = 0; index < changes.size(); ++index) {
        const auto &changed = changes[index];
        SCOPED_TRACE(index);
        const auto destination = temporary.path() / ("normal-" + std::to_string(index));
        auto resolved = resolveAndFetchProjectSources({ changed }, cache, destination, true, *lock);
        ASSERT_TRUE(resolved) << resolved.error().message();
        EXPECT_FALSE(resolved->changed);
        auto outputHash = fileHash(destination / sourceOutputName(changed));
        ASSERT_TRUE(outputHash) << outputHash.error().message();
        EXPECT_EQ(*outputHash, *source.digest);

        const auto lockedDestination = temporary.path() / ("locked-" + std::to_string(index));
        auto verified =
          verifyAndFetchProjectSources({ changed }, cache, lockedDestination, true, *lock);
        ASSERT_TRUE(verified) << verified.error().message();
        auto lockedOutputHash = fileHash(lockedDestination / sourceOutputName(changed));
        ASSERT_TRUE(lockedOutputHash) << lockedOutputHash.error().message();
        EXPECT_EQ(*lockedOutputHash, *source.digest);
    }
    EXPECT_EQ(nlohmann::json(*lock).dump(), previousLock);
    EXPECT_EQ(downloadRequests(), (std::vector<std::string>{ *source.url }));
}

TEST(ProjectLock, DependencyPlansFollowDeclarationsAndMode)
{
    const LockedDependency base{ "org.test.Base",
                                 { { "binary" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    const auto lock = completeProjectLock({}, base);
    auto unchanged = planProjectDependencies(base.requested, std::nullopt, lock, false);
    ASSERT_TRUE(unchanged);
    EXPECT_FALSE(unchanged->changed);
    ASSERT_TRUE(unchanged->base);
    EXPECT_TRUE(std::holds_alternative<LockedDependencyRequest>(*unchanged->base));
    EXPECT_FALSE(unchanged->runtime);
    auto verified = planProjectDependencies(base.requested, std::nullopt, lock, true);
    ASSERT_TRUE(verified);
    EXPECT_FALSE(verified->changed);

    auto changed =
      planProjectDependencies(std::string{ "org.test.Base/2.0" }, std::nullopt, lock, false);
    ASSERT_TRUE(changed);
    EXPECT_TRUE(changed->changed);
    ASSERT_TRUE(changed->base);
    ASSERT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*changed->base));
    EXPECT_EQ(std::get<ResolveDependencyRequest>(*changed->base).requested, "org.test.Base/2.0");

    auto rejected =
      planProjectDependencies(std::string{ "org.test.Base/2.0" }, std::nullopt, lock, true);
    EXPECT_FALSE(rejected);

    auto forced = planProjectDependencies(base.requested, std::nullopt, std::nullopt, false);
    ASSERT_TRUE(forced);
    EXPECT_TRUE(forced->changed);
    ASSERT_TRUE(forced->base);
    EXPECT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*forced->base));

    const LockedDependency runtime{ "org.test.Runtime",
                                    { { "binary" }, "main:org.test.Runtime/1.0.0" },
                                    "runtime" };
    const auto runtimeLock = completeProjectLock({}, base, runtime);
    auto removed = planProjectDependencies(base.requested, std::nullopt, runtimeLock, false);
    ASSERT_TRUE(removed);
    EXPECT_TRUE(removed->changed);
    EXPECT_FALSE(removed->runtime);
    EXPECT_FALSE(planProjectDependencies(base.requested, std::nullopt, runtimeLock, true));
    auto added = planProjectDependencies(base.requested, runtime.requested, lock, false);
    ASSERT_TRUE(added);
    EXPECT_TRUE(added->changed);
    ASSERT_TRUE(added->runtime);
    EXPECT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*added->runtime));
    EXPECT_FALSE(planProjectDependencies(base.requested, runtime.requested, lock, true));
}

TEST(ProjectLock, DependencyPlansDeferImplicitBaseUntilRuntimeMetadataIsAvailable)
{
    const LockedDependency base{ "org.test.Base/1.0",
                                 { { "binary" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    const LockedDependency runtime{ "org.test.Runtime/1.0",
                                    { { "binary" }, "main:org.test.Runtime/1.0.0" },
                                    "runtime" };
    const auto lock = completeProjectLock({}, base, runtime);
    for (const bool locked : { false, true }) {
        auto plan = planProjectDependencies(std::nullopt, runtime.requested, lock, locked);
        ASSERT_TRUE(plan);
        EXPECT_FALSE(plan->base);
        ASSERT_TRUE(plan->runtime);
        EXPECT_TRUE(std::holds_alternative<LockedDependencyRequest>(*plan->runtime));
        EXPECT_FALSE(plan->changed);
    }

    auto fresh = planProjectDependencies(std::nullopt, runtime.requested, std::nullopt, false);
    ASSERT_TRUE(fresh);
    EXPECT_TRUE(fresh->changed);
    EXPECT_FALSE(fresh->base);
    ASSERT_TRUE(fresh->runtime);
    EXPECT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*fresh->runtime));

    const std::string newRuntime = "org.test.Runtime/2.0";
    auto newPlan = planProjectDependencies(std::nullopt, newRuntime, lock, false);
    ASSERT_TRUE(newPlan);
    EXPECT_TRUE(newPlan->changed);
    EXPECT_FALSE(newPlan->base);
    ASSERT_TRUE(newPlan->runtime);
    EXPECT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*newPlan->runtime));
    EXPECT_FALSE(planProjectDependencies(std::nullopt, newRuntime, lock, true));

    auto added = planProjectDependencies(std::nullopt,
                                         runtime.requested,
                                         completeProjectLock({}, base),
                                         false);
    ASSERT_TRUE(added);
    EXPECT_TRUE(added->changed);
    EXPECT_FALSE(added->base);

    auto complete = planProjectDependencies(base.requested, runtime.requested, lock, true);
    ASSERT_TRUE(complete);
    EXPECT_FALSE(complete->changed);
    ASSERT_TRUE(complete->base);
    EXPECT_TRUE(std::holds_alternative<LockedDependencyRequest>(*complete->base));

    auto changed =
      planProjectDependencies(std::string{ "org.test.Base/2.0" }, runtime.requested, lock, false);
    ASSERT_TRUE(changed);
    EXPECT_TRUE(changed->changed);
    ASSERT_TRUE(changed->base);
    EXPECT_TRUE(std::holds_alternative<ResolveDependencyRequest>(*changed->base));
    EXPECT_FALSE(
      planProjectDependencies(std::string{ "org.test.Base/2.0" }, runtime.requested, lock, true));
    EXPECT_FALSE(planProjectDependencies(std::nullopt, std::nullopt, std::nullopt, false));
    EXPECT_FALSE(planProjectDependencies(std::nullopt, std::nullopt, lock, true));
}

TEST(ProjectLock, RejectsIncompleteDependencyLock)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    std::ofstream(projectLockPath(manifest)) << "version: 1\n"
                                                "sources: []\n"
                                                "dependencies:\n"
                                                "  - type: base\n"
                                                "    requested: org.test.Base\n"
                                                "    resolved:\n"
                                                "      reference: main:org.test.Base/1.0.0\n";

    auto locked = prepareSourcesFromManifest({},
                                             manifest,
                                             temporary.path() / "cache",
                                             SourcePreparationMode::Locked);
    ASSERT_FALSE(locked.has_value());
}

TEST(ProjectLock, RecordsOnlyAvailableModules)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    auto generated = prepareSourcesFromManifest({},
                                                manifest,
                                                temporary.path() / "cache",
                                                SourcePreparationMode::Generate);
    ASSERT_TRUE(generated);
    const LockedDependency base{ "org.test.Base",
                                 { { "binary" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    ASSERT_TRUE(writeCompleteProjectLock(manifest, completeProjectLock(generated->entries, base)));
    auto loaded = prepareSourcesFromManifest({},
                                             manifest,
                                             temporary.path() / "cache",
                                             SourcePreparationMode::Locked);
    ASSERT_TRUE(loaded);
    auto lock = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(lock);
    auto lockedBase = lockedDependency(*lock, "base");
    ASSERT_TRUE(lockedBase);
    EXPECT_EQ(lockedBase->resolved.modules, (std::vector<std::string>{ "binary" }));
}

TEST(ProjectLock, DependencyStateTracksOnlyResolvedLayers)
{
    const LockedDependency base{ "org.test.Base/1.0",
                                 { { "binary", "develop" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    const LockedDependency runtime{ "org.test.Runtime/2.0",
                                    { { "binary" }, "main:org.test.Runtime/2.0.0" },
                                    "runtime" };
    const auto original = completeProjectLock({}, base, runtime);
    auto previous = projectLockDependencyState(original);
    ASSERT_TRUE(previous) << previous.error().message();

    auto declarationsChanged = original;
    declarationsChanged.dependencies[0].requested =
      "main:org.test.Base/1.0.0/" + package::Architecture::currentCPUArchitecture().toString();
    declarationsChanged.dependencies[1].requested =
      "main:org.test.Runtime/2.0.0/" + package::Architecture::currentCPUArchitecture().toString();
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/changed-source";
    declarationsChanged.sources = { { { "sha256:" + std::string(64, 'a') }, source } };
    std::reverse(declarationsChanged.dependencies.begin(), declarationsChanged.dependencies.end());
    std::reverse(declarationsChanged.dependencies[1].resolved.modules.begin(),
                 declarationsChanged.dependencies[1].resolved.modules.end());
    auto unchanged = projectLockDependencyState(declarationsChanged);
    ASSERT_TRUE(unchanged) << unchanged.error().message();
    EXPECT_EQ(*unchanged, *previous);

    for (const auto &change : { "base", "runtime", "modules", "remove-runtime" }) {
        SCOPED_TRACE(change);
        auto changed = original;
        if (std::string(change) == "base") {
            changed.dependencies[0].resolved.reference = "main:org.test.Base/1.0.1";
        } else if (std::string(change) == "runtime") {
            changed.dependencies[1].resolved.reference = "main:org.test.Runtime/2.0.1";
        } else if (std::string(change) == "modules") {
            changed.dependencies[0].resolved.modules = { "binary" };
        } else {
            changed.dependencies.pop_back();
        }
        auto state = projectLockDependencyState(changed);
        ASSERT_TRUE(state) << state.error().message();
        EXPECT_NE(*state, *previous);
    }
}

TEST(ProjectLock, DependencyStateIncludesActualArchitecture)
{
    const LockedDependency base{ "org.test.Base/1.0",
                                 { { "binary" }, "main:org.test.Base/1.0.0/unknown" },
                                 "base" };
    const LockedDependency runtime{ "org.test.Runtime/2.0",
                                    { { "binary" }, "main:org.test.Runtime/2.0.0/unknown" },
                                    "runtime" };
    const auto sharedLock = completeProjectLock({}, base, runtime);
    const auto sharedState = projectLockDependencyState(sharedLock);
    ASSERT_TRUE(sharedState) << sharedState.error().message();
    const auto host = package::Architecture::currentCPUArchitecture().toString();
    const auto parsed = nlohmann::json::parse(*sharedState);
    EXPECT_EQ(parsed["base"]["reference"], "main:org.test.Base/1.0.0/" + host);
    EXPECT_EQ(parsed["runtime"]["reference"], "main:org.test.Runtime/2.0.0/" + host);

    std::vector<std::string> states;
    for (const std::string architecture : { "x86_64", "arm64" }) {
        auto lock = sharedLock;
        lock.dependencies[0].resolved.reference = "main:org.test.Base/1.0.0/" + architecture;
        lock.dependencies[1].resolved.reference = "main:org.test.Runtime/2.0.0/" + architecture;
        auto state = projectLockDependencyState(lock);
        ASSERT_TRUE(state) << state.error().message();
        EXPECT_EQ(*state == *sharedState, architecture == host);
        states.push_back(*state);
    }
    EXPECT_NE(states[0], states[1]);
}

TEST(ProjectLock, FailedUpdatePreservesPreviousLock)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    auto generated = prepareSourcesFromManifest({},
                                                manifest,
                                                temporary.path() / "cache",
                                                SourcePreparationMode::Generate);
    ASSERT_TRUE(generated);
    const LockedDependency base{ "org.test.Base",
                                 { { "binary", "develop" }, "main:org.test.Base/1.0.0" },
                                 "base" };
    ASSERT_TRUE(writeCompleteProjectLock(manifest, completeProjectLock(generated->entries, base)));
    const auto before = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(before) << before.error().message();
    const auto previous = projectLockDependencyState(*before);
    ASSERT_TRUE(previous);

    api::types::v1::BuilderProjectSource invalid;
    invalid.kind = "file";
    invalid.url = "https://example.com/source";
    invalid.name = "invalid/name";
    auto updated = prepareSourcesFromManifest({ invalid },
                                              manifest,
                                              temporary.path() / "cache",
                                              SourcePreparationMode::Update);
    EXPECT_FALSE(updated.has_value());
    const auto after = readProjectLock(projectLockPath(manifest));
    ASSERT_TRUE(after) << after.error().message();
    const auto current = projectLockDependencyState(*after);
    ASSERT_TRUE(current);
    EXPECT_EQ(*previous, *current);
}

TEST(ProjectLock, RejectsInvalidSources)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";

    std::vector<std::pair<api::types::v1::BuilderProjectSource, std::string>> cases;

    api::types::v1::BuilderProjectSource unsupported;
    unsupported.kind = "unknown";
    unsupported.url = "https://example.com/source";
    cases.emplace_back(std::move(unsupported), "unsupported source kind");

    api::types::v1::BuilderProjectSource missingUrl;
    missingUrl.kind = "file";
    missingUrl.name = "source";
    cases.emplace_back(std::move(missingUrl), "source URL is missing");

    api::types::v1::BuilderProjectSource missingCommit;
    missingCommit.kind = "git";
    missingCommit.url = "https://example.com/source.git";
    cases.emplace_back(std::move(missingCommit), "git source commit is missing");

    api::types::v1::BuilderProjectSource gitDigest;
    gitDigest.kind = "git";
    gitDigest.url = "https://example.com/source.git";
    gitDigest.commit = "main";
    gitDigest.digest = std::string(64, '0');
    cases.emplace_back(std::move(gitDigest), "git source does not support digest");

    api::types::v1::BuilderProjectSource invalidDigest;
    invalidDigest.kind = "file";
    invalidDigest.url = "https://example.com/source";
    invalidDigest.digest = "invalid";
    cases.emplace_back(std::move(invalidDigest), "source digest must be a SHA-256 value");

    api::types::v1::BuilderProjectSource invalidName;
    invalidName.kind = "file";
    invalidName.url = "https://example.com/source";
    invalidName.name = "directory/source";
    cases.emplace_back(std::move(invalidName), "source name must be a single path component");

    api::types::v1::BuilderProjectSource emptyUrlName;
    emptyUrlName.kind = "file";
    emptyUrlName.url = "https://example.com/directory/";
    cases.emplace_back(std::move(emptyUrlName), "source name must be a single path component");

    api::types::v1::BuilderProjectSource emptyGitName;
    emptyGitName.kind = "git";
    emptyGitName.url = "https://example.com/.git";
    emptyGitName.commit = "main";
    cases.emplace_back(std::move(emptyGitName), "source name must be a single path component");

    for (const auto &[source, expectedError] : cases) {
        SCOPED_TRACE(expectedError);
        auto result = prepareSourcesFromManifest({ source },
                                                 manifest,
                                                 temporary.path() / "cache",
                                                 SourcePreparationMode::Generate);
        ASSERT_FALSE(result.has_value());
        EXPECT_NE(result.error().message().find(expectedError), std::string::npos)
          << result.error().message();
    }
}

TEST(ProjectLock, DuplicateFetchRequestsUseNormalizedSourceFields)
{
    for (const auto &kind : { "git", "file", "archive", "dsc" }) {
        SCOPED_TRACE(kind);
        api::types::v1::BuilderProjectSource first;
        first.kind = kind;
        first.url = "https://example.com/source";
        first.name = "first";
        first.version = "old description";
        first.commit = "main";
        if (first.kind != "git") {
            first.digest = std::string(64, 'A');
            first.submodules = false;
        }

        auto second = first;
        second.name = "second";
        second.version = "new description";
        second.submodules = true;
        if (second.kind != "git") {
            second.digest = "sha256:" + std::string(64, 'a');
            second.commit = "ignored for non-Git sources";
        }

        api::types::v1::BuilderProjectLock lock{};
        const auto hash = first.kind == "git" ? "git-commit:" + std::string(40, 'a')
                                              : "sha256:" + std::string(64, 'a');
        lock.sources = { { { hash }, first }, { { hash }, second } };

        for (const auto &sources : { std::vector{ first, second }, std::vector{ second, first } }) {
            auto generated = resolveProjectLockSources(sources, std::nullopt);
            ASSERT_FALSE(generated);
            EXPECT_NE(generated.error().message().find("duplicate source fetch request"),
                      std::string::npos)
              << generated.error().message();

            auto reused = resolveProjectLockSources(sources, lock);
            ASSERT_FALSE(reused);
            EXPECT_NE(reused.error().message().find("duplicate source fetch request"),
                      std::string::npos)
              << reused.error().message();

            auto verified = verifyProjectLockSources(sources, lock);
            ASSERT_FALSE(verified);
            EXPECT_NE(verified.error().message().find("duplicate source fetch request"),
                      std::string::npos)
              << verified.error().message();
        }
    }
}

TEST(ProjectLock, DistinctFetchRequestsRemainValid)
{
    for (const auto &kind : { "git", "file", "archive", "dsc" }) {
        SCOPED_TRACE(kind);
        api::types::v1::BuilderProjectSource first;
        first.kind = kind;
        first.url = "https://example.com/source";
        first.name = "first";
        if (first.kind == "git") {
            first.commit = "main";
            first.submodules = true;
        } else {
            first.digest = "sha256:" + std::string(64, 'a');
        }

        std::vector<api::types::v1::BuilderProjectSource> changes;
        auto changed = first;
        changed.url = "https://example.com/other-source";
        changes.push_back(changed);
        changed = first;
        changed.kind = first.kind == "file" ? "archive" : "file";
        changes.push_back(changed);
        changed = first;
        if (first.kind == "git") {
            changed.commit = "other";
            changes.push_back(changed);
            changed = first;
            changed.submodules = false;
        } else {
            changed.digest = "sha256:" + std::string(64, 'b');
        }
        changes.push_back(changed);

        for (auto second : changes) {
            second.name = "second";
            SCOPED_TRACE(nlohmann::json(second).dump());
            auto plan = resolveProjectLockSources(std::vector{ first, second }, std::nullopt);
            ASSERT_TRUE(plan) << plan.error().message();
            EXPECT_EQ(plan->requests.size(), 2);

            const auto hash = [](const auto &source) {
                return source.kind == "git" ? "git-commit:" + std::string(40, 'a')
                                            : "sha256:" + std::string(64, 'a');
            };
            api::types::v1::BuilderProjectLock lock{};
            lock.sources = { { { hash(first) }, first }, { { hash(second) }, second } };
            auto verified = verifyProjectLockSources(std::vector{ second, first }, lock);
            ASSERT_TRUE(verified) << verified.error().message();
            EXPECT_EQ(verified->requests.size(), 2);
        }
    }
}

TEST(ProjectLock, ReportsDifferentSourceFieldsConsistently)
{
    TempDir temporary;
    const auto manifest = temporary.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    std::ofstream(projectLockPath(manifest))
      << "version: 1\n"
         "dependencies:\n"
         "  - type: base\n"
         "    requested: org.test.Base\n"
         "    resolved:\n"
         "      reference: main:org.test.Base/1.0.0\n"
         "      modules: [binary, develop]\n"
         "sources:\n"
         "  - source:\n"
         "      kind: file\n"
         "      url: https://example.com/old-source\n"
         "    resolved:\n"
         "      hash: sha256:0000000000000000000000000000000000000000000000000000000000000000\n";

    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/new-source";
    auto result = prepareSourcesFromManifest({ source },
                                             manifest,
                                             temporary.path() / "cache",
                                             SourcePreparationMode::Locked);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find(
                "sources differ from linglong.lock; run ll-builder update-lockfile"),
              std::string::npos)
      << result.error().message();
    std::error_code ec;
    EXPECT_FALSE(std::filesystem::exists(temporary.path() / "cache", ec));
    EXPECT_FALSE(ec);
}

TEST_F(ProjectLockSourceTest, UppercaseManifestDigestMatchesDownloadedContents)
{
    auto source = fileSource("source", "contents");
    std::transform(source.digest->begin() + 7,
                   source.digest->end(),
                   source.digest->begin() + 7,
                   [](unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    auto lock = generateLock({ source });
    ASSERT_TRUE(lock) << lock.error().message();
    EXPECT_TRUE(
      verifyAndFetchProjectSources({ source }, cache, temporary.path() / "locked", true, *lock));
}

TEST_F(ProjectLockSourceTest, DscCacheSupportsDifferentUrlBasenamesAndRelativeRoots)
{
    const auto commands = temporary.path() / "bin";
    std::ofstream(commands / "dget") << R"shell(#!/bin/sh
set -eu
/bin/cp -- "$3" "$(basename "$3")"
)shell";
    std::ofstream(commands / "dscverify") << "#!/bin/sh\ntest -f \"$2\"\n";
    std::ofstream(commands / "dpkg-source") << "#!/bin/sh\nmkdir \"$4\"\n";
    for (const auto &command : { "dget", "dscverify", "dpkg-source" }) {
        std::filesystem::permissions(commands / command, std::filesystem::perms::owner_all);
    }

    auto first = fileSource("first.dsc", "identical dsc contents");
    first.kind = "dsc";
    first.name = "first";
    auto second = first;
    second.name = "second";
    second.url = (temporary.path() / "second.dsc").string();
    std::filesystem::copy_file(*first.url, *second.url);

    for (const bool relative : { false, true }) {
        SCOPED_TRACE(relative);
        const auto root = temporary.path() / (relative ? "relative-cache" : "absolute-cache");
        const auto cachePath = relative ? std::filesystem::relative(root) : root;
        SourceCacheLayout layout(cachePath);
        ASSERT_TRUE(layout.ensure());
        auto destination = utils::TemporaryDirectory::create("fetch-", layout.temporary());
        ASSERT_TRUE(destination) << destination.error().message();

        ProjectSourcePlan generation;
        generation.requests = { GenerateSourceRequest{ first } };
        auto fetched = fetchProjectSources(generation, cachePath, destination->path(), false);
        ASSERT_TRUE(fetched) << fetched.error().message();
        const auto cached = layout.object("dsc", fetched->front().resolved.hash);
        ASSERT_TRUE(std::filesystem::exists(cached / "source.dsc"));
        EXPECT_TRUE(std::filesystem::is_directory(destination->path() / *first.name));

        ProjectSourcePlan locked;
        locked.requests = { LockedSourceRequest{ first, fetched->front().resolved } };
        ASSERT_TRUE(fetchProjectSources(locked, cachePath, destination->path(), true));

        generation.requests = { GenerateSourceRequest{ second } };
        ASSERT_TRUE(fetchProjectSources(generation, cachePath, destination->path(), false));
        EXPECT_TRUE(std::filesystem::is_directory(destination->path() / *second.name));
        locked.requests.emplace_back(LockedSourceRequest{ second, fetched->front().resolved });
        EXPECT_TRUE(fetchProjectSources(locked, cachePath, destination->path(), true));
    }
}

} // namespace linglong::builder
