// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "../mocks/linglong_builder_mock.h"
#include "linglong/builder/builder_directory.h"
#include "linglong/builder/linglong_builder.h"
#include "linglong/builder/source_cache.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/file.h"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

static void expectProjectProcessSuccess(pid_t pid)
{
    ASSERT_GT(pid, 0);
    int status = 0;
    pid_t waited;
    do {
        waited = ::waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);

    ASSERT_EQ(waited, pid);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(LinglongBuilder, YamlPathPreservesFileSymlinksForExplicitAndDefaultLookup)
{
    TempDir workingDir;
    TempDir sharedConfig;
    const auto target = sharedConfig.path() / "app.yaml";
    const auto manifest = workingDir.path() / "linglong.yaml";
    std::ofstream(target) << "shared configuration";
    std::filesystem::create_symlink(target, manifest);

    const auto implicit = linglong::builder::getProjectYamlPath(workingDir.path(), {});
    const auto explicitPath = linglong::builder::getProjectYamlPath(workingDir.path(), manifest);
    const auto relativePath = linglong::builder::getProjectYamlPath(
      workingDir.path(),
      std::filesystem::relative(workingDir.path()) / "linglong.yaml");
    ASSERT_TRUE(implicit);
    ASSERT_TRUE(explicitPath);
    ASSERT_TRUE(relativePath);
    EXPECT_EQ(*implicit, manifest);
    EXPECT_EQ(*explicitPath, manifest);
    EXPECT_EQ(*relativePath, manifest);
    EXPECT_EQ(linglong::builder::projectLockPath(*implicit), workingDir.path() / "linglong.lock");
    EXPECT_EQ(linglong::builder::projectLockPath(*explicitPath),
              workingDir.path() / "linglong.lock");

    std::ifstream input(*explicitPath);
    std::string content;
    std::getline(input, content);
    EXPECT_EQ(content, "shared configuration");

    const auto alias = sharedConfig.path() / "project-alias";
    std::filesystem::create_directory_symlink(workingDir.path(), alias);
    const auto aliased = linglong::builder::getProjectYamlPath(alias, alias / "linglong.yaml");
    ASSERT_TRUE(aliased);
    EXPECT_EQ(*aliased, manifest);
}

TEST(LinglongBuilder, YamlPathPrefersArchitectureEntryAndRejectsMissingFiles)
{
    TempDir workingDir;
    TempDir sharedConfig;
    const auto target = sharedConfig.path() / "app.yaml";
    const auto manifest = workingDir.path()
      / ("linglong." + linglong::package::Architecture::currentCPUArchitecture().toString()
         + ".yaml");
    std::ofstream(target) << "shared configuration";
    std::ofstream(workingDir.path() / "linglong.yaml") << "fallback configuration";
    std::filesystem::create_symlink(target, manifest);

    const auto implicit = linglong::builder::getProjectYamlPath(workingDir.path(), {});
    const auto explicitPath = linglong::builder::getProjectYamlPath(workingDir.path(), manifest);
    ASSERT_TRUE(implicit);
    ASSERT_TRUE(explicitPath);
    EXPECT_EQ(*implicit, manifest);
    EXPECT_EQ(*explicitPath, manifest);

    EXPECT_FALSE(
      linglong::builder::getProjectYamlPath(workingDir.path(), workingDir.path() / "missing.yaml"));
    EXPECT_FALSE(linglong::builder::getProjectYamlPath(workingDir.path(),
                                                       workingDir.path() / "missing-directory"
                                                         / "linglong.yaml"));
    const auto brokenLink = workingDir.path() / "broken.yaml";
    std::filesystem::create_symlink(sharedConfig.path() / "missing.yaml", brokenLink);
    EXPECT_FALSE(linglong::builder::getProjectYamlPath(workingDir.path(), brokenLink));
}

TEST(LinglongBuilder, BuilderLocksYamlEntryDirectoryAcrossWorkingDirectories)
{
    TempDir workingDir;
    TempDir sharedConfig;
    const auto yamlDirectory = workingDir.path() / "config";
    const auto manifest = yamlDirectory / "linglong.yaml";
    const auto target = sharedConfig.path() / "app.yaml";
    std::filesystem::create_directory(yamlDirectory);
    std::ofstream(target) << "shared configuration";
    std::filesystem::create_symlink(target, manifest);

    {
        linglong::builder::BuilderMock builder(workingDir.path());
        builder.projectYamlFile = manifest.string();
        ASSERT_TRUE(builder.cleanBuildArtifacts());
        ASSERT_TRUE(builder.cleanBuildArtifacts());

        for (const auto &directory : { workingDir.path(), yamlDirectory }) {
            const auto pid = ::fork();
            if (pid == 0) {
                ::alarm(10);
                linglong::builder::BuilderMock competing(directory);
                competing.projectYamlFile = manifest.string();
                auto result = competing.cleanBuildArtifacts();
                ::_exit(!result
                            && result.error().message().find("already in use") != std::string::npos
                          ? 0
                          : 1);
            }
            expectProjectProcessSuccess(pid);
        }

        // The target's directory has its own resource lock and remains independent.
        auto independent =
          linglong::builder::BuilderDirectoryLayout(sharedConfig.path()).lockProject();
        ASSERT_TRUE(independent) << independent.error().message();
    }

    auto projectLock = linglong::builder::BuilderDirectoryLayout(workingDir.path()).lockProject();
    auto yamlLock = linglong::builder::BuilderDirectoryLayout(yamlDirectory).lockProject();
    ASSERT_TRUE(projectLock) << projectLock.error().message();
    ASSERT_TRUE(yamlLock) << yamlLock.error().message();
}

TEST(LinglongBuilder, BuilderDeduplicatesWorkingAndYamlDirectoryAliases)
{
    TempDir workingDir;
    TempDir aliases;
    const auto alias = aliases.path() / "project";
    std::filesystem::create_directory_symlink(workingDir.path(), alias);
    linglong::builder::BuilderMock builder(workingDir.path());
    builder.projectYamlFile = (alias / "linglong.yaml").string();

    ASSERT_TRUE(builder.cleanBuildArtifacts());
    ASSERT_TRUE(builder.cleanBuildArtifacts());
}

TEST(LinglongBuilder, BuilderReleasesWorkingDirectoryLockWhenYamlDirectoryIsBusy)
{
    TempDir workingDir;
    const auto yamlDirectory = workingDir.path() / "config";
    std::filesystem::create_directory(yamlDirectory);
    auto held = linglong::builder::BuilderDirectoryLayout(yamlDirectory).lockProject();
    ASSERT_TRUE(held);
    // Initialize shared mock resources before forking.
    linglong::builder::BuilderMock builder(workingDir.path());
    builder.projectYamlFile = (yamlDirectory / "linglong.yaml").string();

    const auto pid = ::fork();
    if (pid == 0) {
        ::alarm(10);
        auto result = builder.cleanBuildArtifacts();
        if (result || result.error().message().find("already in use") == std::string::npos) {
            ::_exit(1);
        }

        // The failed Builder is still alive; it must not retain the first lock.
        auto released = linglong::builder::BuilderDirectoryLayout(workingDir.path()).lockProject();
        ::_exit(released && released->isLocked() ? 0 : 2);
    }
    expectProjectProcessSuccess(pid);
}

TEST(LinglongBuilder, ProjectLockRejectsConcurrentProcessesAndDirectoryAliases)
{
    TempDir workingDir;
    TempDir otherProject;
    const auto project = workingDir.path() / "project";
    const auto alias = workingDir.path() / "alias";
    std::filesystem::create_directory(project);
    std::filesystem::create_directory_symlink(project, alias);

    auto lock = linglong::builder::BuilderDirectoryLayout(project).lockProject();
    ASSERT_TRUE(lock) << lock.error().message();
    ASSERT_TRUE(lock->isLocked());

    for (const auto &path : { project, project / ".", alias }) {
        const auto pid = ::fork();
        if (pid == 0) {
            ::alarm(10);
            auto conflicting = linglong::builder::BuilderDirectoryLayout(path).lockProject();
            if (conflicting
                || conflicting.error().message().find("already in use") == std::string::npos) {
                ::_exit(1);
            }

            auto independent =
              linglong::builder::BuilderDirectoryLayout(otherProject.path()).lockProject();
            ::_exit(independent && independent->isLocked() ? 0 : 2);
        }
        expectProjectProcessSuccess(pid);
    }
}

TEST(LinglongBuilder, ProjectLockSurvivesCleanAndCanBeAcquiredAfterRelease)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    ASSERT_TRUE(directories.ensureRoot());
    std::ofstream(directories.root() / "state") << "build state";

    std::optional<linglong::utils::filelock::FileLock> held;
    auto lock = directories.lockProject();
    ASSERT_TRUE(lock) << lock.error().message();
    held.emplace(std::move(*lock));

    ASSERT_TRUE(directories.clean());
    EXPECT_FALSE(std::filesystem::exists(directories.root()));
    EXPECT_TRUE(std::filesystem::is_regular_file(workingDir.path() / ".ll-builder.lock"));

    const auto pid = ::fork();
    if (pid == 0) {
        ::alarm(10);
        auto conflicting = directories.lockProject();
        ::_exit(!conflicting
                    && conflicting.error().message().find("already in use") != std::string::npos
                  ? 0
                  : 1);
    }
    expectProjectProcessSuccess(pid);

    held.reset();
    auto acquired = directories.lockProject();
    ASSERT_TRUE(acquired) << acquired.error().message();
    EXPECT_TRUE(acquired->isLocked());
}

TEST(LinglongBuilder, ProjectLockIsReleasedWhenProcessExits)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    const auto pid = ::fork();
    if (pid == 0) {
        auto lock = directories.lockProject();
        // Exit without destructors to verify that a leftover file does not keep the project busy.
        ::_exit(lock && lock->isLocked() ? 0 : 1);
    }
    expectProjectProcessSuccess(pid);

    ASSERT_TRUE(std::filesystem::is_regular_file(workingDir.path() / ".ll-builder.lock"));
    auto acquired = directories.lockProject();
    ASSERT_TRUE(acquired) << acquired.error().message();
    EXPECT_TRUE(acquired->isLocked());
}

TEST(LinglongBuilder, ProjectOperationsRejectBusyProjectBeforeDoingWork)
{
    TempDir workingDir;
    linglong::builder::BuilderMock builder(linglong::api::types::v1::BuilderProject{},
                                           workingDir.path());
    auto lock = linglong::builder::BuilderDirectoryLayout(workingDir.path()).lockProject();
    ASSERT_TRUE(lock);

    for (int operation = 0; operation < 8; ++operation) {
        SCOPED_TRACE(operation);
        const auto pid = ::fork();
        if (pid == 0) {
            ::alarm(10);
            linglong::utils::error::Result<void> result;
            switch (operation) {
            case 0:
                result = builder.build();
                break;
            case 1:
                result = builder.cleanBuildArtifacts();
                break;
            case 2:
                result = builder.run({}, {});
                break;
            case 3:
                result = builder.exportUAB({});
                break;
            case 4:
                result = builder.exportLayer({});
                break;
            case 5:
                result = builder.push("binary");
                break;
            case 6:
                result = builder.processProjectLock(linglong::builder::ProjectLockMode::Generate);
                break;
            case 7:
                result = builder.processProjectLock(linglong::builder::ProjectLockMode::Update);
                break;
            }

            ::_exit(!result && result.error().message().find("already in use") != std::string::npos
                      ? 0
                      : 1);
        }
        expectProjectProcessSuccess(pid);
    }
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / ".uabBuild"));
}

TEST(LinglongBuilder, BuilderReusesProjectLockUntilItsResourcesAreDestroyed)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    {
        linglong::builder::BuilderMock builder(linglong::api::types::v1::BuilderProject{},
                                               workingDir.path());
        // A failed operation still leaves the instance holding its project lock.
        EXPECT_FALSE(builder.build());
        ASSERT_TRUE(builder.cleanBuildArtifacts());
        ASSERT_TRUE(builder.cleanBuildArtifacts());

        const auto pid = ::fork();
        if (pid == 0) {
            ::alarm(10);
            auto conflicting = directories.lockProject();
            ::_exit(!conflicting
                        && conflicting.error().message().find("already in use") != std::string::npos
                      ? 0
                      : 1);
        }
        expectProjectProcessSuccess(pid);
    }

    auto acquired = directories.lockProject();
    ASSERT_TRUE(acquired) << acquired.error().message();
    EXPECT_TRUE(acquired->isLocked());
}

TEST(LinglongBuilder, LockedBuildRejectsMissingLockBeforeCreatingState)
{
    // Include the preparation error in build()'s stage error message.
    linglong::utils::EnvironmentVariableGuard backtrace("LINYAPS_BACKTRACE", "1");
    TempDir workingDir;
    linglong::api::types::v1::BuilderProject project;
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
    linglong::builder::BuilderBuildOptions options;
    options.locked = true;
    builder.setBuildOptions(options);

    auto result = builder.build();
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("does not exist"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
}

TEST(LinglongBuilder, LockedBuildRejectsSkipFetchBeforeCreatingState)
{
    linglong::utils::EnvironmentVariableGuard backtrace("LINYAPS_BACKTRACE", "1");
    TempDir workingDir;
    linglong::builder::BuilderMock builder(linglong::api::types::v1::BuilderProject{},
                                           workingDir.path());
    builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();
    linglong::builder::BuilderBuildOptions options;
    options.locked = true;
    options.skipFetchSource = true;
    builder.setBuildOptions(options);

    auto result = builder.build();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("--locked cannot be combined with --skip-fetch-source"),
              std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
}

TEST(LinglongBuilder, NormalBuildWithoutLockContinuesProjectValidation)
{
    linglong::utils::EnvironmentVariableGuard backtrace("LINYAPS_BACKTRACE", "1");
    TempDir workingDir;
    linglong::api::types::v1::BuilderProject project;
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();

    auto result = builder.build();
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("invalid project info"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
}

TEST(LinglongBuilder, BuildsRejectInvalidExistingLockBeforeCreatingState)
{
    linglong::utils::EnvironmentVariableGuard backtrace("LINYAPS_BACKTRACE", "1");
    for (const int mode : { 0, 1, 2 }) {
        SCOPED_TRACE(mode);
        TempDir workingDir;
        const auto manifest = workingDir.path() / "linglong.yaml";
        std::ofstream(linglong::builder::projectLockPath(manifest)) << "invalid lock";
        linglong::api::types::v1::BuilderProject project;
        linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
        builder.projectYamlFile = manifest.string();
        linglong::builder::BuilderBuildOptions options;
        options.locked = mode == 1;
        options.skipFetchSource = mode == 2;
        builder.setBuildOptions(options);

        auto result = builder.build();
        ASSERT_FALSE(result);
        EXPECT_NE(result.error().message().find("failed to read"), std::string::npos);
        EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
    }
}

TEST(LinglongBuilder, GenerateLockRejectsExistingFileBeforeCreatingState)
{
    TempDir workingDir;
    const auto manifest = workingDir.path() / "linglong.yaml";
    const auto lockPath = linglong::builder::projectLockPath(manifest);
    std::ofstream(lockPath) << "invalid lock";
    linglong::api::types::v1::BuilderProject project;
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = manifest.string();

    auto result = builder.processProjectLock(linglong::builder::ProjectLockMode::Generate);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("already exists"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
    std::ifstream lockFile(lockPath);
    std::string content;
    std::getline(lockFile, content);
    EXPECT_EQ(content, "invalid lock");
}

TEST(LinglongBuilder, UpdateLockRequiresExistingFileBeforeCreatingState)
{
    TempDir workingDir;
    linglong::api::types::v1::BuilderProject project;
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = (workingDir.path() / "linglong.yaml").string();

    auto result = builder.processProjectLock(linglong::builder::ProjectLockMode::Update);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("does not exist"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
}

TEST(LinglongBuilder, UpdateLockIgnoresOldContentAndPreservesItOnFailure)
{
    TempDir workingDir;
    const auto manifest = workingDir.path() / "linglong.yaml";
    const auto lockPath = linglong::builder::projectLockPath(manifest);
    std::ofstream(lockPath) << "invalid lock";
    linglong::api::types::v1::BuilderProject project;
    linglong::api::types::v1::BuilderProjectSource source;
    source.kind = "unsupported";
    project.sources = std::vector{ source };
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = manifest.string();
    linglong::utils::EnvironmentVariableGuard cache("LINGLONG_FETCH_CACHE",
                                                    (workingDir.path() / "cache").string());

    auto result = builder.processProjectLock(linglong::builder::ProjectLockMode::Update);
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find("unsupported source kind"), std::string::npos);
    std::ifstream lockFile(lockPath);
    std::string content;
    std::getline(lockFile, content);
    EXPECT_EQ(content, "invalid lock");
}

TEST(LinglongBuilder, SkipFetchPreservesManualSourcesDespiteExistingLock)
{
    TempDir workingDir;
    const auto manifest = workingDir.path() / "linglong.yaml";
    std::ofstream(manifest) << "version: 1\n";
    const auto sourceDirectory = workingDir.path() / "linglong/sources";
    std::filesystem::create_directories(sourceDirectory);
    const auto manualSource = sourceDirectory / "manual-source";
    std::ofstream(manualSource) << "local changes";
    const auto lockPath = linglong::builder::projectLockPath(manifest);
    std::ofstream(lockPath) << "invalid lock";

    linglong::api::types::v1::BuilderProject project;
    linglong::api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/source.git";
    source.commit = "0123456789012345678901234567890123456789";
    project.sources = std::vector{ source };
    linglong::builder::BuilderMock builder(std::move(project), workingDir.path());
    builder.projectYamlFile = manifest.string();
    linglong::builder::BuilderBuildOptions options;
    options.skipFetchSource = true;
    builder.setBuildOptions(options);

    auto skipped = builder.buildStageFetchSource();
    ASSERT_TRUE(skipped.has_value()) << skipped.error().message();
    EXPECT_TRUE(std::filesystem::exists(manualSource));
    std::ifstream lockFile(lockPath);
    std::string lockContent;
    std::getline(lockFile, lockContent);
    EXPECT_EQ("invalid lock", lockContent);
}

TEST(LinglongBuilder, BuildOverlaysPreserveHistoryUntilDependenciesChange)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    const auto baseUpper = directories.buildBaseOverlay() / "upperdir";
    const auto runtimeUpper = directories.buildRuntimeOverlay() / "upperdir";
    auto prepared = linglong::builder::detail::prepareBuildOverlays(directories, "first", true);
    ASSERT_TRUE(prepared) << prepared.error().message();
    EXPECT_FALSE(*prepared);
    ASSERT_TRUE(std::filesystem::is_directory(baseUpper));
    ASSERT_TRUE(std::filesystem::is_directory(runtimeUpper));
    std::ofstream(baseUpper / "installed-tool") << "APT installation";
    std::ofstream(runtimeUpper / "build-history") << "build changes";
    std::filesystem::create_directories(directories.sources());
    std::ofstream(directories.sources() / "source") << "source changes";

    auto reused = linglong::builder::detail::prepareBuildOverlays(directories, "first", true);
    ASSERT_TRUE(reused) << reused.error().message();
    EXPECT_TRUE(*reused);
    EXPECT_TRUE(std::filesystem::exists(baseUpper / "installed-tool"));
    EXPECT_TRUE(std::filesystem::exists(runtimeUpper / "build-history"));

    auto changed = linglong::builder::detail::prepareBuildOverlays(directories, "second", true);
    ASSERT_TRUE(changed) << changed.error().message();
    EXPECT_FALSE(*changed);
    EXPECT_TRUE(std::filesystem::is_directory(baseUpper));
    EXPECT_TRUE(std::filesystem::is_directory(runtimeUpper));
    EXPECT_FALSE(std::filesystem::exists(baseUpper / "installed-tool"));
    EXPECT_FALSE(std::filesystem::exists(runtimeUpper / "build-history"));
    EXPECT_TRUE(std::filesystem::exists(directories.sources() / "source"));
    auto state = linglong::utils::readFile(directories.buildOverlayState());
    ASSERT_TRUE(state);
    EXPECT_EQ(*state, "second");

    std::ofstream(baseUpper / "new-build-history") << "build changes";
    ASSERT_TRUE(directories.clean());
    EXPECT_FALSE(std::filesystem::exists(directories.overlays()));
    auto cleaned = linglong::builder::detail::prepareBuildOverlays(directories, "second", true);
    ASSERT_TRUE(cleaned);
    EXPECT_FALSE(*cleaned);
    EXPECT_FALSE(std::filesystem::exists(baseUpper / "new-build-history"));
}

TEST(LinglongBuilder, BuildOverlaysResetWhenRuntimeIsAddedOrRemoved)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    const auto baseUpper = directories.buildBaseOverlay() / "upperdir";
    const auto runtimeOverlay = directories.buildRuntimeOverlay();
    ASSERT_TRUE(linglong::builder::detail::prepareBuildOverlays(directories, "base", false));
    std::ofstream(baseUpper / "old-history") << "base changes";
    EXPECT_FALSE(std::filesystem::exists(runtimeOverlay));

    auto added = linglong::builder::detail::prepareBuildOverlays(directories, "base-runtime", true);
    ASSERT_TRUE(added);
    EXPECT_FALSE(*added);
    EXPECT_FALSE(std::filesystem::exists(baseUpper / "old-history"));
    ASSERT_TRUE(std::filesystem::is_directory(runtimeOverlay / "upperdir"));
    std::ofstream(runtimeOverlay / "upperdir/history") << "runtime changes";

    auto removed = linglong::builder::detail::prepareBuildOverlays(directories, "base", false);
    ASSERT_TRUE(removed);
    EXPECT_FALSE(*removed);
    EXPECT_FALSE(std::filesystem::exists(runtimeOverlay));
}

TEST(LinglongBuilder, BuildOverlaysResetWhenStateOrRequiredDirectoriesAreMissing)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());
    const auto baseUpper = directories.buildBaseOverlay() / "upperdir";
    const auto runtimeUpper = directories.buildRuntimeOverlay() / "upperdir";
    ASSERT_TRUE(linglong::builder::detail::prepareBuildOverlays(directories, "dependencies", true));
    std::ofstream(baseUpper / "history") << "build changes";
    ASSERT_TRUE(std::filesystem::remove(directories.buildOverlayState()));
    auto unknown =
      linglong::builder::detail::prepareBuildOverlays(directories, "dependencies", true);
    ASSERT_TRUE(unknown);
    EXPECT_FALSE(*unknown);
    EXPECT_FALSE(std::filesystem::exists(baseUpper / "history"));

    std::ofstream(baseUpper / "history") << "build changes";
    ASSERT_TRUE(std::filesystem::remove(runtimeUpper));
    auto incomplete =
      linglong::builder::detail::prepareBuildOverlays(directories, "dependencies", true);
    ASSERT_TRUE(incomplete);
    EXPECT_FALSE(*incomplete);
    EXPECT_FALSE(std::filesystem::exists(baseUpper / "history"));
    EXPECT_TRUE(std::filesystem::is_directory(runtimeUpper));
}

TEST(LinglongBuilder, ResetsOverlaysWhenDependencyArchitectureChanges)
{
    TempDir temporary;
    const linglong::builder::BuilderDirectoryLayout directories(temporary.path());
    const linglong::builder::LockedDependency base{ "org.test.Base/1.0.0",
                                                    { { "binary" }, "main:org.test.Base/1.0.0.0" },
                                                    "base" };
    auto lock = linglong::builder::completeProjectLock({}, base);
    for (const auto &name : { "x86_64", "arm64" }) {
        lock.dependencies[0].resolved.reference = base.resolved.reference + "/" + name;
        auto state = linglong::builder::projectLockDependencyState(lock);
        ASSERT_TRUE(state) << state.error().message();
        auto prepared = linglong::builder::detail::prepareBuildOverlays(directories, *state, false);
        ASSERT_TRUE(prepared) << prepared.error().message();
        EXPECT_FALSE(*prepared);
        const auto history = directories.buildBaseOverlay() / "upperdir/history";
        EXPECT_FALSE(std::filesystem::exists(history));
        std::ofstream(history) << name;
        auto reused = linglong::builder::detail::prepareBuildOverlays(directories, *state, false);
        ASSERT_TRUE(reused);
        EXPECT_TRUE(*reused);
        EXPECT_TRUE(std::filesystem::exists(history));
    }
}

TEST(LinglongBuilder, installModule)
{
    TempDir buildOutput;
    TempDir moduleOutput;

    // Create test files and directories
    const auto &buildPath = buildOutput.path();
    std::filesystem::create_directories(buildPath / "include");
    std::filesystem::create_directories(buildPath / "lib/debug");
    std::filesystem::create_directories(buildPath / "bin");
    std::filesystem::create_directories(buildPath / "share/icons/hicolor/48x48/apps");

    std::ofstream(buildPath / "include/header.h");
    std::ofstream(buildPath / "lib/libfoo.a");
    std::ofstream(buildPath / "lib/libfoo.so");
    std::ofstream(buildPath / "lib/debug/libfoo.so.debug");
    std::ofstream(buildPath / "bin/tool");
    std::ofstream(buildPath / "share/app.desktop");
    std::ofstream(buildPath / "share/icons/hicolor/48x48/apps/app.png");

    // Define install rules
    std::unordered_set<std::string> rules = {
        "^/include/.+",  // regex rule
        "/lib/libfoo.a", // normal path rule
        "/bin/tool",
        "^/share/icons/.+", // regex rule for icons
        "# a comment",      // comment rule
        ""                  // empty rule
    };

    auto result = linglong::builder::installModule(buildOutput.path(), moduleOutput.path(), rules);
    ASSERT_TRUE(result.has_value());

    const auto &modulePath = moduleOutput.path();
    // check files are moved
    EXPECT_TRUE(std::filesystem::exists(modulePath / "include/header.h"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "lib/libfoo.a"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "bin/tool"));
    EXPECT_TRUE(std::filesystem::exists(modulePath / "share/icons/hicolor/48x48/apps/app.png"));

    // check files are NOT moved
    EXPECT_FALSE(std::filesystem::exists(modulePath / "lib/libfoo.so"));
    EXPECT_FALSE(std::filesystem::exists(modulePath / "lib/debug/libfoo.so.debug"));
    EXPECT_FALSE(std::filesystem::exists(modulePath / "share/app.desktop"));

    // check original files are removed (because they are moved)
    EXPECT_FALSE(std::filesystem::exists(buildPath / "include/header.h"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "lib/libfoo.a"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "bin/tool"));
    EXPECT_FALSE(std::filesystem::exists(buildPath / "share/icons/hicolor/48x48/apps/app.png"));

    // check files that should not be moved are still there
    EXPECT_TRUE(std::filesystem::exists(buildPath / "lib/libfoo.so"));
    EXPECT_TRUE(std::filesystem::exists(buildPath / "lib/debug/libfoo.so.debug"));
    EXPECT_TRUE(std::filesystem::exists(buildPath / "share/app.desktop"));

    // Check returned installed file list
    std::vector<std::string> expectedFiles = { "bin",
                                               "bin/tool",
                                               "include",
                                               "include/header.h",
                                               "lib",
                                               "lib/libfoo.a",
                                               "share",
                                               "share/icons",
                                               "share/icons/hicolor",
                                               "share/icons/hicolor/48x48",
                                               "share/icons/hicolor/48x48/apps",
                                               "share/icons/hicolor/48x48/apps/app.png" };
    std::vector<std::string> installedFiles;
    for (const auto &p : *result) {
        installedFiles.emplace_back(p);
    }
    std::sort(installedFiles.begin(), installedFiles.end());

    std::sort(expectedFiles.begin(), expectedFiles.end());

    EXPECT_EQ(installedFiles, expectedFiles);
}

TEST(LinglongBuilder, mergeOutput)
{
    TempDir srcDir1;
    TempDir srcDir2;
    TempDir destDir;

    // Create test files and directories
    const auto &srcPath1 = srcDir1.path();
    std::filesystem::create_directories(srcPath1 / "bin");
    std::filesystem::create_directories(srcPath1 / "lib");
    std::filesystem::create_directories(srcPath1 / "lib/systemd");
    std::filesystem::create_directories(srcPath1 / "share");

    std::ofstream(srcPath1 / "bin/tool1");
    std::ofstream(srcPath1 / "lib/libfoo.so");
    std::ofstream(srcPath1 / "lib/systemd/dbus.service");
    std::ofstream(srcPath1 / "share/data1");
    std::ofstream(srcPath1 / ".wh.somefile");

    const auto &srcPath2 = srcDir2.path();
    std::filesystem::create_directories(srcPath2 / "bin");
    std::filesystem::create_directories(srcPath2 / "lib");
    std::filesystem::create_directories(srcPath2 / "share");

    std::ofstream(srcPath2 / "bin/tool2");
    std::ofstream(srcPath2 / "lib/libbar.so");
    std::ofstream(srcPath2 / "share/data2");

    // Define merge targets
    std::vector<std::string> targets = { "bin/", "share/data1" };

    auto result = linglong::builder::detail::mergeOutput({ srcPath1, srcPath2 },
                                                         destDir.path(),
                                                         targets,
                                                         { "lib/systemd" });
    ASSERT_TRUE(result) << result.error().message();

    const auto &destPath = destDir.path();
    // Check files are merged
    EXPECT_TRUE(std::filesystem::exists(destPath / "bin/tool1"));
    EXPECT_TRUE(std::filesystem::exists(destPath / "bin/tool2"));
    EXPECT_TRUE(std::filesystem::exists(destPath / "share/data1"));

    // Check files are NOT merged
    EXPECT_FALSE(std::filesystem::exists(destPath / "lib/libfoo.so"));
    EXPECT_FALSE(std::filesystem::exists(destPath / "lib/libbar.so"));
    EXPECT_FALSE(std::filesystem::exists(destPath / "share/data2"));
    EXPECT_FALSE(std::filesystem::exists(destPath / ".wh.somefile"));
    EXPECT_FALSE(std::filesystem::exists(destPath / "lib/systemd/dbus.service"));
}

TEST(LinglongBuilder, mergeOutputReportsMissingSource)
{
    TempDir temporary;
    const auto source = temporary.path() / "upper";
    std::filesystem::create_directories(source / "bin");
    std::ofstream(source / "bin/tool") << "tool";
    const auto destination = temporary.path() / "output";

    auto result = linglong::builder::detail::mergeOutput({ temporary.path() / "missing", source },
                                                         destination,
                                                         { "bin/" },
                                                         {});
    ASSERT_FALSE(result);
    EXPECT_NE(result.error().message().find((temporary.path() / "missing").string()),
              std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(destination));
}

TEST(LinglongBuilder, mergeOutputReportsCopyFailures)
{
    TempDir temporary;
    const auto source = temporary.path() / "upper";
    const auto destination = temporary.path() / "output";
    std::filesystem::create_directories(source / "bin");
    std::filesystem::create_directories(destination);
    std::ofstream(source / "bin/tool") << "tool";
    std::ofstream(destination / "bin") << "existing file";

    auto result = linglong::builder::detail::mergeOutput({ source }, destination, { "bin/" }, {});
    ASSERT_FALSE(result);
    EXPECT_TRUE(std::filesystem::is_regular_file(destination / "bin"));
}

TEST(LinglongBuilder, UabExportFilename)
{
    linglong::builder::BuilderMock builder;
    auto ref =
      linglong::package::Reference::parse(std::string("main:org.deepin.demo/1.0.0.0/arm64"));
    EXPECT_TRUE(ref.has_value()) << ref.error().message();
    auto filename = builder.uabExportFilename(*ref, linglong::builder::ExportMode::Distribution);
    EXPECT_EQ(filename, "org.deepin.demo_1.0.0.0_arm64_main.uab");
    filename = builder.uabExportFilename(*ref, linglong::builder::ExportMode::Exec);
    EXPECT_EQ(filename, "org.deepin.demo_1.0.0.0_arm64_main.uabx");
};

TEST(LinglongBuilder, LayerExportFilename)
{
    linglong::builder::BuilderMock builder;
    auto ref =
      linglong::package::Reference::parse(std::string("main:org.deepin.demo/1.0.0.0/arm64"));
    EXPECT_TRUE(ref.has_value()) << ref.error().message();
    auto filename = builder.layerExportFilename(*ref, "binary");
    EXPECT_EQ(filename, "org.deepin.demo_1.0.0.0_arm64_binary.layer");
};

TEST(BuilderDirectoryLayout, ResetsStageDirectoriesAndPreservesCache)
{
    TempDir workingDir;
    linglong::builder::BuilderDirectoryLayout directories(workingDir.path());

    EXPECT_EQ(directories.root(), workingDir.path() / "linglong");
    EXPECT_EQ(directories.cache(), directories.root() / "cache");
    EXPECT_EQ(directories.sources(), directories.root() / "sources");
    EXPECT_EQ(directories.buildOutput(), directories.root() / "output/_build");
    EXPECT_EQ(directories.buildBaseOverlay(), directories.overlays() / "build_base");
    EXPECT_EQ(directories.buildRuntimeOverlay(), directories.overlays() / "build_runtime");
    EXPECT_EQ(directories.prepareBaseOverlay(), directories.overlays() / "prepare_base");
    EXPECT_EQ(directories.prepareRuntimeOverlay(), directories.overlays() / "prepare_runtime");
    EXPECT_EQ(directories.buildOverlayState(), directories.overlays() / "build_overlay.state");
    EXPECT_EQ(directories.module("binary"), directories.root() / "output/binary");

    ASSERT_TRUE(directories.ensureRoot());
    EXPECT_TRUE(std::filesystem::exists(directories.root()));
    EXPECT_FALSE(std::filesystem::exists(directories.cache()));

    linglong::builder::SourceCacheLayout sourceCache(directories.cache());
    ASSERT_TRUE(sourceCache.ensure());
    EXPECT_TRUE(std::filesystem::exists(directories.cache()));
    std::ofstream(directories.cache() / "persistent") << "cache";
    std::filesystem::create_directories(directories.sources());
    std::ofstream(directories.sources() / "stale") << "source";
    ASSERT_TRUE(directories.resetSources());
    EXPECT_TRUE(std::filesystem::exists(directories.sources()));
    EXPECT_FALSE(std::filesystem::exists(directories.sources() / "stale"));
    EXPECT_TRUE(std::filesystem::exists(directories.cache() / "persistent"));

    std::filesystem::create_directories(directories.output());
    std::ofstream(directories.output() / "stale") << "output";
    ASSERT_TRUE(directories.resetOutput());
    EXPECT_TRUE(std::filesystem::exists(directories.buildOutput()));
    EXPECT_FALSE(std::filesystem::exists(directories.output() / "stale"));

    ASSERT_TRUE(directories.clean());
    EXPECT_FALSE(std::filesystem::exists(directories.root()));
}

TEST(LinglongBuilder, CleanNoBuildDir)
{
    TempDir workingDir;

    linglong::builder::BuilderMock builder(workingDir.path());
    auto result = builder.cleanBuildArtifacts();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(std::filesystem::exists(workingDir.path() / "linglong"));
}

TEST(LinglongBuilder, CleanNormal)
{
    TempDir workingDir;

    linglong::builder::BuilderMock builder(workingDir.path());

    auto linglongDir = workingDir.path() / "linglong";
    std::filesystem::create_directories(linglongDir / "cache" / "overlay" / "workdir");
    std::filesystem::create_directories(linglongDir / "output");
    std::ofstream(linglongDir / "cache" / "overlay" / "workdir" / "work");
    std::ofstream(linglongDir / "output" / "binary");

    ASSERT_TRUE(std::filesystem::exists(linglongDir));

    auto result = builder.cleanBuildArtifacts();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(std::filesystem::exists(linglongDir));
}

TEST(LinglongBuilder, CleanWithPermissionIssue)
{
    if (geteuid() == 0) {
        GTEST_SKIP() << "root can remove files regardless of the owner permission bits";
    }

    TempDir workingDir;

    linglong::builder::BuilderMock builder(workingDir.path());

    namespace fs = std::filesystem;
    auto linglongDir = workingDir.path() / "linglong";
    auto blockedDir = linglongDir / "cache" / "overlay" / "workdir" / "work";
    fs::create_directories(blockedDir);

    // Simulate the overlayfs workdir permission issue: d---------
    fs::permissions(blockedDir, fs::perms::none, fs::perm_options::replace);

    // Verify the permission issue exists
    auto blockedPerms = fs::symlink_status(blockedDir).permissions();
    EXPECT_EQ(blockedPerms
                & (fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec),
              fs::perms::none);

    // Builder::cleanBuildArtifacts should fix permissions and remove the directory
    auto result = builder.cleanBuildArtifacts();
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_FALSE(fs::exists(linglongDir));
}
