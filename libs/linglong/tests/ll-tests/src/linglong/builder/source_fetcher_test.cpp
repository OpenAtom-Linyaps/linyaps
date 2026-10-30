// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/builder/source_fetcher.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/env.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/filelock.h"
#include "linglong/utils/unique_fd.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <tuple>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace linglong;

namespace linglong::builder {

namespace {

class FetchProcess
{
public:
    explicit FetchProcess(const std::function<int()> &fetch)
    {
        std::array<int, 2> completion;
        if (::pipe2(completion.data(), O_CLOEXEC) != 0) {
            throw std::runtime_error("failed to create fetch completion pipe");
        }
        completed.reset(completion[0]);
        utils::fd::UniqueFd notify(completion[1]);

        pid = ::fork();
        if (pid < 0) {
            throw std::runtime_error("failed to fork source fetch process");
        }
        if (pid == 0) {
            ::setpgid(0, 0);
            completed.reset();
            const auto result = fetch();
            const char done = '1';
            if (::write(notify.get(), &done, 1) != 1) {
                ::_exit(1);
            }
            ::_exit(result);
        }
        ::setpgid(pid, pid);
    }

    ~FetchProcess()
    {
        if (pid > 0) {
            // Also stop helper commands if a test fails while a fetch is paused.
            ::kill(-pid, SIGKILL);
            while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) { }
        }
    }

    int wait()
    {
        pollfd event{ completed.get(), POLLIN, 0 };
        if (::poll(&event, 1, 10000) != 1 || !(event.revents & POLLIN)) {
            return -1;
        }

        int status = 0;
        pid_t waited;
        do {
            waited = ::waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited != pid) {
            return -1;
        }
        pid = -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

private:
    pid_t pid = -1;
    utils::fd::UniqueFd completed;
};

class FetchGate
{
public:
    explicit FetchGate(const std::filesystem::path &directory)
        : readyPath(directory / "ready")
        , resumePath(directory / "resume")
    {
        std::filesystem::create_directories(directory);
        if (::mkfifo(readyPath.c_str(), 0600) != 0 || ::mkfifo(resumePath.c_str(), 0600) != 0) {
            throw std::runtime_error("failed to create source fetch gate");
        }
        ready.reset(::open(readyPath.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC));
        resume.reset(::open(resumePath.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC));
        if (!ready || !resume) {
            throw std::runtime_error("failed to open source fetch gate");
        }
    }

    bool wait()
    {
        pollfd event{ ready.get(), POLLIN, 0 };
        return ::poll(&event, 1, 10000) == 1 && (event.revents & POLLIN);
    }

    bool release() { return ::write(resume.get(), "1\n", 2) == 2; }

    const std::filesystem::path readyPath;
    const std::filesystem::path resumePath;

private:
    utils::fd::UniqueFd ready;
    utils::fd::UniqueFd resume;
};

} // namespace

auto prepareSourceDirectories(const std::filesystem::path &cache,
                              std::initializer_list<std::filesystem::path> destinations)
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("prepare test source directories");

    auto ensured = SourceCacheLayout(cache).ensure();
    if (!ensured) {
        return LINGLONG_ERR(ensured);
    }

    for (const auto &destination : destinations) {
        std::error_code ec;
        std::filesystem::create_directories(destination, ec);
        if (ec) {
            return LINGLONG_ERR("failed to create test source destination: " + ec.message());
        }
    }

    return LINGLONG_OK;
}

class SourceFetcherTest : public ::testing::Test
{
protected:
    void SetUp() override { }
};

auto lockedRequest(api::types::v1::BuilderProjectSource source, std::string hash)
  -> LockedSourceRequest
{
    api::types::v1::BuilderSourceLockResolved resolved;
    resolved.hash = std::move(hash);
    return LockedSourceRequest{ std::move(source), std::move(resolved) };
}

std::string commandOutputLine(std::string output)
{
    const auto end = output.find_first_of("\r\n");
    if (end != std::string::npos) {
        output.erase(end);
    }
    return output;
}

class SourceCacheProcessTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(prepareSourceDirectories(cache, { firstDestination, secondDestination }));
        std::ofstream(artifact) << "source contents";
        auto digest = fileHash(artifact);
        ASSERT_TRUE(digest) << digest.error().message();
        hash = *digest;
        source.kind = "file";
        source.url = artifact.string();
        source.digest = hash;
        source.name = "source";

        auto tarPath = utils::Cmd("sh").exec({ "-c", "command -v tar" });
        if (tarPath) {
            realTar = std::filesystem::absolute(commandOutputLine(*tarPath));
        }
        const auto commands = temporary.path() / "bin";
        std::filesystem::create_directories(commands);
        std::ofstream(commands / "wget") << R"(#!/bin/sh
set -eu
/bin/cp -- "$1" "$3"
if [ "$LINGLONG_TEST_FETCH_GATE" = download ]; then
    printf 'ready\n' > "$LINGLONG_TEST_FETCH_READY"
    read -r resume < "$LINGLONG_TEST_FETCH_RESUME"
fi
)";
        std::ofstream(commands / "tar") << R"(#!/bin/sh
set -eu
printf 'ready\n' > "$LINGLONG_TEST_FETCH_READY"
read -r resume < "$LINGLONG_TEST_FETCH_RESUME"
if [ "$LINGLONG_TEST_FETCH_FAIL" = 1 ]; then
    exit 1
fi
exec "$LINGLONG_TEST_REAL_TAR" "$@"
)";
        for (const auto &command : { "wget", "tar" }) {
            std::filesystem::permissions(commands / command, std::filesystem::perms::owner_all);
        }

        const auto *originalPath = std::getenv("PATH");
        commandPath = std::make_unique<utils::EnvironmentVariableGuard>(
          "PATH",
          commands.string() + ":" + (originalPath ? originalPath : ""));
    }

    int fetch(const FetchGate &gate,
              const std::filesystem::path &destination,
              bool offline,
              bool failMaterialization = false,
              bool locked = true)
    {
        utils::EnvironmentVariableGuard ready("LINGLONG_TEST_FETCH_READY", gate.readyPath.string());
        utils::EnvironmentVariableGuard resume("LINGLONG_TEST_FETCH_RESUME",
                                               gate.resumePath.string());
        utils::EnvironmentVariableGuard tar("LINGLONG_TEST_REAL_TAR", realTar.string());
        utils::EnvironmentVariableGuard failure("LINGLONG_TEST_FETCH_FAIL",
                                                failMaterialization ? "1" : "0");
        utils::EnvironmentVariableGuard phase("LINGLONG_TEST_FETCH_GATE",
                                              source.kind == "archive" ? "materialize"
                                                                       : "download");
        SourceFetchRequest request = GenerateSourceRequest{ source };
        if (locked) {
            request = lockedRequest(source, hash);
        }
        SourceFetcher fetcher(std::move(request), SourceCacheLayout(cache));
        auto result = fetcher.fetch(destination, offline);
        return result.has_value() == !failMaterialization && (!result || result->hash == hash) ? 0
                                                                                               : 1;
    }

    TempDir temporary;
    const std::filesystem::path cache = temporary.path() / "cache";
    const std::filesystem::path artifact = temporary.path() / "artifact";
    const std::filesystem::path firstDestination = temporary.path() / "first";
    const std::filesystem::path secondDestination = temporary.path() / "second";
    const SourceCacheLayout layout{ cache };
    api::types::v1::BuilderProjectSource source;
    std::string hash;
    std::filesystem::path realTar;
    std::unique_ptr<utils::EnvironmentVariableGuard> commandPath;
};

TEST_F(SourceCacheProcessTest, RechecksCacheAfterConcurrentDownloadsAndRepairsAcrossProcesses)
{
    const auto cached = layout.object(source.kind, hash);
    std::filesystem::create_directories(cached.parent_path());
    std::ofstream(cached) << "corrupted cache";

    FetchGate firstGate(temporary.path() / "first-gate");
    FetchGate secondGate(temporary.path() / "second-gate");
    FetchProcess first([&]() {
        return fetch(firstGate, firstDestination, false);
    });
    FetchProcess second([&]() {
        return fetch(secondGate, secondDestination, false);
    });
    ASSERT_TRUE(firstGate.wait());
    ASSERT_TRUE(secondGate.wait());

    // Both downloads have finished, but neither holds the cache lock during acquisition.
    {
        auto probe = utils::filelock::FileLock::create(layout.lockFile(cached),
                                                       utils::filelock::LockType::Write);
        ASSERT_TRUE(probe) << probe.error().message();
        auto locked = probe->tryLock(utils::filelock::LockType::Write);
        ASSERT_TRUE(locked) << locked.error().message();
        ASSERT_TRUE(*locked);
    }

    ASSERT_TRUE(secondGate.release());
    ASSERT_EQ(second.wait(), 0);
    ASSERT_EQ(*fileHash(cached), hash);
    struct stat published{};
    ASSERT_EQ(::stat(cached.c_str(), &published), 0);

    // The first fetch observed corruption before downloading. It must now reuse the repair.
    ASSERT_TRUE(firstGate.release());
    ASSERT_EQ(first.wait(), 0);
    struct stat reused{};
    ASSERT_EQ(::stat(cached.c_str(), &reused), 0);
    EXPECT_EQ(reused.st_ino, published.st_ino);
    EXPECT_EQ(*fileHash(cached), hash);
    EXPECT_EQ(*fileHash(firstDestination / "source"), hash);
    EXPECT_EQ(*fileHash(secondDestination / "source"), hash);
}

class SourceCacheMaterializationTest
    : public SourceCacheProcessTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
};

TEST_P(SourceCacheMaterializationTest, HoldsEntryLockThroughMaterializationAndReleasesOnReturn)
{
    if (realTar.empty()) {
        GTEST_SKIP() << "tar is not available";
    }
    const auto [locked, offline, failMaterialization] = GetParam();
    const auto content = temporary.path() / "content";
    std::filesystem::create_directories(content);
    std::ofstream(content / "source.txt") << "archive contents";
    auto archived =
      utils::Cmd(realTar.string()).exec({ "-cf", artifact.string(), "-C", content.string(), "." });
    ASSERT_TRUE(archived) << archived.error().message();
    auto digest = fileHash(artifact);
    ASSERT_TRUE(digest) << digest.error().message();
    hash = *digest;
    source.kind = "archive";
    source.digest = hash;
    const auto cached = layout.object(source.kind, hash);
    if (offline) {
        std::filesystem::create_directories(cached.parent_path());
        std::filesystem::copy_file(artifact, cached);
    }

    FetchGate gate(temporary.path() / "materialize-gate");
    FetchProcess reader([&]() {
        return fetch(gate, firstDestination, offline, failMaterialization, locked);
    });
    ASSERT_TRUE(gate.wait());

    // The archive helper is paused inside materialize; another process cannot replace the cache.
    const auto lockPath = layout.lockFile(cached);
    struct stat heldLock{};
    ASSERT_EQ(::stat(lockPath.c_str(), &heldLock), 0);
    auto probe = utils::filelock::FileLock::create(lockPath, utils::filelock::LockType::Write);
    ASSERT_TRUE(probe) << probe.error().message();
    auto busy = probe->tryLock(utils::filelock::LockType::Write);
    ASSERT_TRUE(busy) << busy.error().message();
    EXPECT_FALSE(*busy);

    ASSERT_TRUE(gate.release());
    ASSERT_EQ(reader.wait(), 0);
    auto available = probe->tryLock(utils::filelock::LockType::Write);
    ASSERT_TRUE(available) << available.error().message();
    EXPECT_TRUE(*available);
    struct stat releasedLock{};
    ASSERT_EQ(::stat(lockPath.c_str(), &releasedLock), 0);
    EXPECT_EQ(releasedLock.st_ino, heldLock.st_ino);
    EXPECT_EQ(std::filesystem::exists(firstDestination / "source/source.txt"),
              !failMaterialization);
    EXPECT_EQ(*fileHash(cached), hash);
}

INSTANTIATE_TEST_SUITE_P(MaterializationResult,
                         SourceCacheMaterializationTest,
                         ::testing::Values(std::make_tuple(true, false, false),
                                           std::make_tuple(true, false, true),
                                           std::make_tuple(true, true, false),
                                           std::make_tuple(true, true, true),
                                           std::make_tuple(false, false, false),
                                           std::make_tuple(false, false, true)));

TEST(GitSourceHash, ExtractsSha1AndSha256CommitIds)
{
    const std::string sha1(40, 'a');
    const std::string sha256(64, 'B');
    EXPECT_EQ(gitCommitFromHash("git-commit:" + sha1), sha1);
    EXPECT_EQ(gitCommitFromHash("git-commit:" + sha256), sha256);
}

TEST(GitSourceHash, RejectsInvalidPrefixesAndCommitIds)
{
    EXPECT_FALSE(gitCommitFromHash(""));
    EXPECT_FALSE(gitCommitFromHash(std::string(40, 'a')));
    EXPECT_FALSE(gitCommitFromHash("sha256:" + std::string(64, 'a')));
    EXPECT_FALSE(gitCommitFromHash("git-commit:main"));
    EXPECT_FALSE(gitCommitFromHash("git-commit:" + std::string(39, 'a')));
    EXPECT_FALSE(gitCommitFromHash("git-commit:" + std::string(41, 'a')));
    EXPECT_FALSE(gitCommitFromHash("git-commit:" + std::string(40, 'g')));
}

TEST(SourceUrl, ExtractsFileName)
{
    EXPECT_EQ(fileNameFromUrl("https://example.com/releases/source.tar.gz"), "source.tar.gz");
    EXPECT_EQ(fileNameFromUrl("archive.tar.gz"), "archive.tar.gz");
}

TEST(SourceUrl, IgnoresQueryAndFragment)
{
    EXPECT_EQ(fileNameFromUrl("https://example.com/source.tar.gz?download=1#content"),
              "source.tar.gz");
    EXPECT_EQ(fileNameFromUrl("file:///tmp/source.zip#content"), "source.zip");
}

TEST(SourceUrl, DecodesPercentEncodedFileName)
{
    EXPECT_EQ(fileNameFromUrl("https://example.com/source%20archive.tar.gz"),
              "source archive.tar.gz");
}

TEST(SourceUrl, ReturnsEmptyNameForTrailingSlash)
{
    EXPECT_TRUE(fileNameFromUrl("https://example.com/releases/").empty());
}

TEST(SourceOutputName, PreservesDotGitSuffixForGitSource)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/project.git";

    EXPECT_EQ(sourceOutputName(source), "project.git");
}

TEST(SourceOutputName, PreservesExplicitNameForGitSource)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/project.git";
    source.name = "custom.git";

    EXPECT_EQ(sourceOutputName(source), "custom.git");
}

TEST(SourceOutputName, PreservesDotGitSuffixForFileSource)
{
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/project.git";

    EXPECT_EQ(sourceOutputName(source), "project.git");
}

TEST(SourceCache, PublishesGitDirectoryAtomicallyAndCopiesItToBuildDirectory)
{
    utils::Cmd git("git");
    if (!git.exists()) {
        GTEST_SKIP() << "git is not available";
    }

    TempDir temporary;
    const auto repository = temporary.path() / "repository";
    auto initialized = git.exec({ "init", repository.string() });
    ASSERT_TRUE(initialized.has_value()) << initialized.error().message();
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }))
      << "failed to configure git user name";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }))
      << "failed to configure git user email";
    std::ofstream(repository / "source.txt") << "cached content";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "add", "source.txt" }))
      << "failed to stage test source";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "-m", "initial" }))
      << "failed to commit test source";
    auto commit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(commit.has_value()) << commit.error().message();
    commit->erase(commit->find_first_of("\r\n"));

    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = repository.string();
    source.commit = *commit;
    source.name = "source";
    const auto cache = temporary.path() / "cache";
    const auto firstDestination = temporary.path() / "first";
    const auto secondDestination = temporary.path() / "second";
    ASSERT_TRUE(prepareSourceDirectories(cache,
                                         { firstDestination,
                                           secondDestination,
                                           temporary.path() / "offline",
                                           temporary.path() / "dirty",
                                           temporary.path() / "repaired-dirty",
                                           temporary.path() / "repaired-object",
                                           temporary.path() / "corrupt",
                                           temporary.path() / "repaired" }));

    auto fetch = [&source, &cache](const std::filesystem::path &destination) {
        SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cache));
        return fetcher.fetch(destination);
    };
    FetchProcess second([&]() {
        auto result = fetch(secondDestination);
        return result && result->hash == "git-commit:" + *commit ? 0 : 1;
    });
    auto firstResult = fetch(firstDestination);
    ASSERT_EQ(second.wait(), 0);
    ASSERT_TRUE(firstResult.has_value()) << firstResult.error().message();
    EXPECT_EQ(firstResult->hash, "git-commit:" + *commit);

    const auto cached = SourceCacheLayout(cache).gitObject(*commit, true);
    EXPECT_TRUE(std::filesystem::is_directory(cached));

    struct stat cacheStatus{};
    struct stat buildStatus{};
    ASSERT_EQ(::stat((cached / "source.txt").c_str(), &cacheStatus), 0);
    ASSERT_EQ(::stat((firstDestination / "source/source.txt").c_str(), &buildStatus), 0);
    EXPECT_NE(cacheStatus.st_ino, buildStatus.st_ino);
    EXPECT_TRUE(std::filesystem::is_directory(cached / ".git"));
    EXPECT_TRUE(std::filesystem::is_directory(firstDestination / "source/.git"));
    auto firstHead =
      git.exec({ "-C", (firstDestination / "source").string(), "rev-parse", "--verify", "HEAD" });
    ASSERT_TRUE(firstHead.has_value()) << firstHead.error().message();
    EXPECT_TRUE(firstHead->rfind(*commit, 0) == 0);

    std::ofstream(firstDestination / "source/source.txt", std::ios::trunc) << "modified";
    EXPECT_EQ(*fileHash(cached / "source.txt"), *fileHash(repository / "source.txt"));

    SourceFetcher offlineFetcher(lockedRequest(source, firstResult->hash),
                                 SourceCacheLayout(cache));
    auto offline = offlineFetcher.fetch(temporary.path() / "offline", true);
    ASSERT_TRUE(offline.has_value()) << offline.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(temporary.path() / "offline/source/.git"));

    std::ofstream(cached / "source.txt", std::ios::trunc) << "corrupted cached content";
    auto dirty = offlineFetcher.fetch(temporary.path() / "dirty", true);
    ASSERT_TRUE(dirty.has_value()) << dirty.error().message();
    EXPECT_EQ(*fileHash(temporary.path() / "dirty/source/source.txt"),
              *fileHash(repository / "source.txt"));
    EXPECT_NE(*fileHash(cached / "source.txt"), *fileHash(repository / "source.txt"));
    SourceFetcher repairDirty(lockedRequest(source, firstResult->hash), SourceCacheLayout(cache));
    auto repairedDirty = repairDirty.fetch(temporary.path() / "repaired-dirty");
    ASSERT_TRUE(repairedDirty.has_value()) << repairedDirty.error().message();

    auto blob = git.exec({ "-C", cached.string(), "rev-parse", "HEAD:source.txt" });
    ASSERT_TRUE(blob.has_value()) << blob.error().message();
    *blob = commandOutputLine(std::move(*blob));
    const auto object = cached / ".git/objects" / blob->substr(0, 2) / blob->substr(2);
    std::filesystem::create_directories(object.parent_path());
    if (std::filesystem::exists(object)) {
        std::filesystem::permissions(object,
                                     std::filesystem::perms::owner_write,
                                     std::filesystem::perm_options::add);
    }
    {
        std::ofstream corruptedObject(object, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(corruptedObject.is_open());
        corruptedObject << "corrupt git object";
    }
    const auto previousOutputHash = fileHash(firstDestination / "source/source.txt");
    ASSERT_TRUE(previousOutputHash.has_value()) << previousOutputHash.error().message();
    auto corruptObject = offlineFetcher.fetch(firstDestination, true);
    EXPECT_FALSE(corruptObject.has_value());
    EXPECT_EQ(*fileHash(firstDestination / "source/source.txt"), *previousOutputHash);
    SourceFetcher repairObject(lockedRequest(source, firstResult->hash), SourceCacheLayout(cache));
    auto repairedObject = repairObject.fetch(temporary.path() / "repaired-object");
    ASSERT_TRUE(repairedObject.has_value()) << repairedObject.error().message();
    EXPECT_EQ(*fileHash(temporary.path() / "repaired-object/source/source.txt"),
              *fileHash(repository / "source.txt"));

    std::error_code ec;
    std::filesystem::remove_all(cached / ".git", ec);
    ASSERT_FALSE(ec) << ec.message();
    auto corrupt = offlineFetcher.fetch(temporary.path() / "corrupt", true);
    EXPECT_FALSE(corrupt.has_value());

    SourceFetcher repairFetcher(lockedRequest(source, firstResult->hash), SourceCacheLayout(cache));
    auto repaired = repairFetcher.fetch(temporary.path() / "repaired");
    ASSERT_TRUE(repaired.has_value()) << repaired.error().message();
    EXPECT_TRUE(std::filesystem::is_directory(cached / ".git"));
}

TEST(SourceCache, CopiesGitTreeWithoutFollowingSymlinksOrCopyingSpecialFiles)
{
    utils::Cmd git("git");
    if (!git.exists()) {
        GTEST_SKIP() << "git is not available";
    }

    TempDir temporary;
    const auto repository = temporary.path() / "repository";
    ASSERT_TRUE(git.exec({ "init", repository.string() }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }));
    ASSERT_TRUE(
      git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }));
    std::ofstream(repository / "source.txt") << "source contents";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "add", "." }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "-m", "initial" }));
    auto commit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(commit) << commit.error().message();
    *commit = commandOutputLine(std::move(*commit));

    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = repository.string();
    source.commit = *commit;
    source.name = "source";
    const auto cache = temporary.path() / "cache";
    const auto destination = temporary.path() / "offline";
    ASSERT_TRUE(prepareSourceDirectories(cache, { temporary.path() / "initial", destination }));
    SourceFetcher initialFetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cache));
    auto initial = initialFetcher.fetch(temporary.path() / "initial");
    ASSERT_TRUE(initial) << initial.error().message();

    // Keep test entries in Git metadata so reset/clean cannot hide copyTree mistakes.
    const auto metadata = SourceCacheLayout(cache).gitObject(*commit, true) / ".git/copy-test";
    std::filesystem::create_directories(metadata / "nested/empty");
    std::ofstream(metadata / "nested/file") << "metadata contents";
    std::filesystem::permissions(metadata / "nested/file", std::filesystem::perms::owner_all);
    const auto external = temporary.path() / "external";
    std::filesystem::create_directories(external);
    std::ofstream(external / "file") << "external contents";
    const std::array<std::pair<std::string, std::filesystem::path>, 5> links{
        { { "file-link", "nested/file" },
          { "directory-link", "nested" },
          { "dangling-link", "missing" },
          { "cycle-link", "." },
          { "external-link", external } }
    };
    for (const auto &[name, target] : links) {
        std::filesystem::create_symlink(target, metadata / name);
    }
    ASSERT_EQ(::mkfifo((metadata / "fifo").c_str(), 0600), 0) << errno;

    utils::fd::UniqueFd socket(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    ASSERT_TRUE(socket) << errno;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    // Bind at a short path to stay within sun_path even when the cache path is long.
    const auto socketPath = (temporary.path() / "socket").string();
    ASSERT_LT(socketPath.size(), sizeof(address.sun_path));
    std::copy(socketPath.begin(), socketPath.end(), address.sun_path);
    ASSERT_EQ(::bind(socket.get(), reinterpret_cast<const sockaddr *>(&address), sizeof(address)),
              0)
      << errno;
    std::filesystem::rename(socketPath, metadata / "fsmonitor.socket");

    SourceFetcher offlineFetcher(lockedRequest(source, initial->hash), SourceCacheLayout(cache));
    auto fetched = offlineFetcher.fetch(destination, true);
    ASSERT_TRUE(fetched) << fetched.error().message();
    EXPECT_EQ(fetched->hash, initial->hash);
    const auto copied = destination / "source/.git/copy-test";
    EXPECT_TRUE(std::filesystem::is_directory(copied / "nested/empty"));
    ASSERT_TRUE(std::filesystem::is_regular_file(copied / "nested/file"));
    EXPECT_EQ(*fileHash(copied / "nested/file"), *fileHash(metadata / "nested/file"));
    EXPECT_EQ(std::filesystem::status(copied / "nested/file").permissions(),
              std::filesystem::status(metadata / "nested/file").permissions());
    for (const auto &[name, target] : links) {
        ASSERT_TRUE(std::filesystem::is_symlink(copied / name)) << name;
        EXPECT_EQ(std::filesystem::read_symlink(copied / name), target) << name;
    }
    EXPECT_FALSE(std::filesystem::exists(copied / "fifo"));
    EXPECT_FALSE(std::filesystem::exists(copied / "fsmonitor.socket"));
    EXPECT_TRUE(std::filesystem::is_fifo(metadata / "fifo"));
    EXPECT_TRUE(std::filesystem::is_socket(metadata / "fsmonitor.socket"));
}

class GitSourceCacheResetTest : public ::testing::TestWithParam<const char *>
{
};

TEST_P(GitSourceCacheResetTest, RestoresHiddenChangesAndCleansExtraFilesOffline)
{
    utils::Cmd git("git");
    if (!git.exists()) {
        GTEST_SKIP() << "git is not available";
    }

    TempDir temporary;
    const auto repository = temporary.path() / "repository";
    ASSERT_TRUE(git.exec({ "init", repository.string() }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }));
    ASSERT_TRUE(
      git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }));
    std::ofstream(repository / "source.txt") << "original";
    std::ofstream(repository / ".gitignore") << "build/\n";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "add", "." }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "-m", "initial" }));
    auto commit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(commit.has_value()) << commit.error().message();
    *commit = commandOutputLine(std::move(*commit));

    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = repository.string();
    source.commit = *commit;
    source.name = "source";
    const auto cache = temporary.path() / "cache";
    ASSERT_TRUE(
      prepareSourceDirectories(cache,
                               { temporary.path() / "initial", temporary.path() / "offline" }));
    SourceFetcher initialFetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cache));
    auto initial = initialFetcher.fetch(temporary.path() / "initial");
    ASSERT_TRUE(initial.has_value()) << initial.error().message();

    const auto cached = SourceCacheLayout(cache).gitObject(*commit, true);
    ASSERT_TRUE(git.exec({ "-C", cached.string(), "update-index", GetParam(), "source.txt" }));
    std::ofstream(cached / "source.txt", std::ios::trunc) << "hidden modification";
    std::filesystem::create_directories(cached / "build");
    std::ofstream(cached / "build/config.h") << "stale configuration";
    ASSERT_TRUE(git.exec({ "init", (cached / "build/nested").string() }));
    auto hiddenStatus = git.exec({ "-C", cached.string(), "status", "--porcelain=v1" });
    ASSERT_TRUE(hiddenStatus.has_value()) << hiddenStatus.error().message();
    EXPECT_TRUE(hiddenStatus->empty());
    std::ofstream(cached / "untracked.txt") << "extra input";

    SourceFetcher offlineFetcher(lockedRequest(source, initial->hash), SourceCacheLayout(cache));
    const auto destination = temporary.path() / "offline";
    auto fetched = offlineFetcher.fetch(destination, true);
    ASSERT_TRUE(fetched.has_value()) << fetched.error().message();
    EXPECT_EQ(fetched->hash, initial->hash);
    EXPECT_EQ(*fileHash(destination / "source/source.txt"), *fileHash(repository / "source.txt"));
    EXPECT_FALSE(std::filesystem::exists(destination / "source/build"));
    EXPECT_FALSE(std::filesystem::exists(destination / "source/untracked.txt"));
    EXPECT_TRUE(std::filesystem::is_directory(destination / "source/.git"));
    EXPECT_NE(*fileHash(cached / "source.txt"), *fileHash(repository / "source.txt"));
    EXPECT_TRUE(std::filesystem::exists(cached / "build/config.h"));
    EXPECT_TRUE(std::filesystem::exists(cached / "build/nested/.git"));
    EXPECT_TRUE(std::filesystem::exists(cached / "untracked.txt"));
}

INSTANTIATE_TEST_SUITE_P(IndexFlags,
                         GitSourceCacheResetTest,
                         ::testing::Values("--assume-unchanged", "--skip-worktree"));

TEST(SourceCache, SeparatesCommitsWithIdenticalWorkingTrees)
{
    utils::Cmd git("git");
    if (!git.exists()) {
        GTEST_SKIP() << "git is not available";
    }

    TempDir temporary;
    const auto repository = temporary.path() / "repository.git";
    ASSERT_TRUE(git.exec({ "init", repository.string() })) << "failed to initialize repository";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }));
    ASSERT_TRUE(
      git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }));
    std::ofstream(repository / "source.txt") << "same tree";
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "add", "source.txt" }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "-m", "first" }));
    auto firstCommit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(firstCommit.has_value()) << firstCommit.error().message();
    *firstCommit = commandOutputLine(std::move(*firstCommit));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "--allow-empty", "-m", "second" }));
    auto secondCommit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(secondCommit.has_value()) << secondCommit.error().message();
    *secondCommit = commandOutputLine(std::move(*secondCommit));
    ASSERT_NE(*firstCommit, *secondCommit);
    ASSERT_TRUE(
      prepareSourceDirectories(temporary.path() / "cache", { temporary.path() / "destination" }));

    auto fetchCommit = [&](const std::string &commit, const std::string &name) {
        api::types::v1::BuilderProjectSource source;
        source.kind = "git";
        source.url = repository.string();
        source.commit = commit;
        source.name = name;
        SourceFetcher fetcher(GenerateSourceRequest{ source },
                              SourceCacheLayout(temporary.path() / "cache"));
        return fetcher.fetch(temporary.path() / "destination");
    };
    auto first = fetchCommit(*firstCommit, "first");
    auto second = fetchCommit(*secondCommit, "second");
    ASSERT_TRUE(first.has_value()) << first.error().message();
    ASSERT_TRUE(second.has_value()) << second.error().message();
    EXPECT_NE(first->hash, second->hash);
    EXPECT_EQ(first->hash, "git-commit:" + *firstCommit);
    EXPECT_EQ(second->hash, "git-commit:" + *secondCommit);

    auto firstHead =
      git.exec({ "-C", (temporary.path() / "destination/first").string(), "rev-parse", "HEAD" });
    auto secondHead =
      git.exec({ "-C", (temporary.path() / "destination/second").string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(firstHead.has_value()) << firstHead.error().message();
    ASSERT_TRUE(secondHead.has_value()) << secondHead.error().message();
    EXPECT_EQ(commandOutputLine(std::move(*firstHead)), *firstCommit);
    EXPECT_EQ(commandOutputLine(std::move(*secondHead)), *secondCommit);

    SourceCacheLayout cache(temporary.path() / "cache");
    EXPECT_TRUE(std::filesystem::is_directory(cache.gitObject(*firstCommit, true) / ".git"));
    EXPECT_TRUE(std::filesystem::is_directory(cache.gitObject(*secondCommit, true) / ".git"));
}

TEST(SourceCache, PreservesSubmoduleGitMetadata)
{
    utils::Cmd git("git");
    if (!git.exists()) {
        GTEST_SKIP() << "git is not available";
    }
    utils::EnvironmentVariableGuard protocol("GIT_ALLOW_PROTOCOL", "file");

    TempDir temporary;
    auto initializeRepository = [&](const std::filesystem::path &repository) {
        EXPECT_TRUE(git.exec({ "init", repository.string() }));
        EXPECT_TRUE(git.exec({ "-C", repository.string(), "config", "user.name", "Test" }));
        EXPECT_TRUE(
          git.exec({ "-C", repository.string(), "config", "user.email", "test@example.com" }));
    };

    const auto grandchild = temporary.path() / "grandchild";
    initializeRepository(grandchild);
    std::ofstream(grandchild / "leaf.txt") << "leaf";
    std::ofstream(grandchild / ".gitignore") << "build/\n";
    ASSERT_TRUE(git.exec({ "-C", grandchild.string(), "add", "." }));
    ASSERT_TRUE(git.exec({ "-C", grandchild.string(), "commit", "-m", "grandchild" }));

    const auto child = temporary.path() / "child";
    initializeRepository(child);
    std::ofstream(child / "child.txt") << "child";
    std::ofstream(child / ".gitignore") << "build/\n";
    ASSERT_TRUE(git.exec({ "-C", child.string(), "add", "." }));
    ASSERT_TRUE(git.exec({ "-C", child.string(), "commit", "-m", "child" }));
    ASSERT_TRUE(git.exec({ "-C",
                           child.string(),
                           "-c",
                           "protocol.file.allow=always",
                           "submodule",
                           "add",
                           grandchild.string(),
                           "nested/grandchild" }));
    ASSERT_TRUE(git.exec({ "-C", child.string(), "commit", "-am", "nested submodule" }));

    const auto repository = temporary.path() / "repository";
    initializeRepository(repository);
    ASSERT_TRUE(git.exec({ "-C",
                           repository.string(),
                           "-c",
                           "protocol.file.allow=always",
                           "submodule",
                           "add",
                           child.string(),
                           "deps/child" }));
    ASSERT_TRUE(git.exec({ "-C", repository.string(), "commit", "-am", "add submodule" }));
    auto commit = git.exec({ "-C", repository.string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(commit.has_value()) << commit.error().message();
    *commit = commandOutputLine(std::move(*commit));

    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = repository.string();
    source.commit = *commit;
    source.name = "source";
    source.submodules = true;
    const auto cachePath = temporary.path() / "cache";
    ASSERT_TRUE(prepareSourceDirectories(cachePath,
                                         { temporary.path() / "destination",
                                           temporary.path() / "offline",
                                           temporary.path() / "without-submodules" }));
    SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cachePath));
    auto fetched = fetcher.fetch(temporary.path() / "destination");
    ASSERT_TRUE(fetched.has_value()) << fetched.error().message();

    const auto checkout = temporary.path() / "destination/source";
    EXPECT_TRUE(std::filesystem::is_directory(checkout / ".git"));
    EXPECT_TRUE(std::filesystem::is_regular_file(checkout / "deps/child/.git"));
    auto childHead = git.exec({ "-C", (checkout / "deps/child").string(), "rev-parse", "HEAD" });
    ASSERT_TRUE(childHead.has_value()) << childHead.error().message();

    const auto cached = SourceCacheLayout(cachePath).gitObject(*commit, true);
    const auto cachedChild = cached / "deps/child";
    const auto cachedGrandchild = cachedChild / "nested/grandchild";
    ASSERT_TRUE(
      git.exec({ "-C", cachedChild.string(), "update-index", "--assume-unchanged", "child.txt" }));
    ASSERT_TRUE(
      git.exec({ "-C", cachedGrandchild.string(), "update-index", "--skip-worktree", "leaf.txt" }));
    std::ofstream(cachedChild / "child.txt", std::ios::trunc) << "hidden child modification";
    std::ofstream(cachedGrandchild / "leaf.txt", std::ios::trunc) << "hidden leaf modification";
    std::filesystem::create_directories(cachedChild / "build");
    std::filesystem::create_directories(cachedGrandchild / "build");
    std::ofstream(cachedChild / "build/config.h") << "stale child configuration";
    std::ofstream(cachedGrandchild / "build/config.h") << "stale leaf configuration";
    std::ofstream(cachedChild / "untracked.txt") << "extra child input";

    SourceFetcher offlineFetcher(lockedRequest(source, fetched->hash),
                                 SourceCacheLayout(cachePath));
    auto offline = offlineFetcher.fetch(temporary.path() / "offline", true);
    ASSERT_TRUE(offline.has_value()) << offline.error().message();
    EXPECT_TRUE(
      std::filesystem::is_regular_file(temporary.path() / "offline/source/deps/child/.git"));
    const auto offlineChild = temporary.path() / "offline/source/deps/child";
    const auto offlineGrandchild = offlineChild / "nested/grandchild";
    EXPECT_EQ(*fileHash(offlineChild / "child.txt"), *fileHash(child / "child.txt"));
    EXPECT_EQ(*fileHash(offlineGrandchild / "leaf.txt"), *fileHash(grandchild / "leaf.txt"));
    EXPECT_FALSE(std::filesystem::exists(offlineChild / "build"));
    EXPECT_FALSE(std::filesystem::exists(offlineGrandchild / "build"));
    EXPECT_FALSE(std::filesystem::exists(offlineChild / "untracked.txt"));
    EXPECT_TRUE(std::filesystem::is_regular_file(offlineGrandchild / ".git"));
    EXPECT_NE(*fileHash(cachedChild / "child.txt"), *fileHash(child / "child.txt"));
    EXPECT_NE(*fileHash(cachedGrandchild / "leaf.txt"), *fileHash(grandchild / "leaf.txt"));
    EXPECT_TRUE(std::filesystem::exists(cachedChild / "build/config.h"));
    EXPECT_TRUE(std::filesystem::exists(cachedGrandchild / "build/config.h"));

    source.submodules = false;
    SourceFetcher noSubmodules(GenerateSourceRequest{ source }, SourceCacheLayout(cachePath));
    auto without = noSubmodules.fetch(temporary.path() / "without-submodules");
    ASSERT_TRUE(without.has_value()) << without.error().message();
    EXPECT_EQ(without->hash, fetched->hash);
    EXPECT_FALSE(
      std::filesystem::exists(temporary.path() / "without-submodules/source/deps/child/child.txt"));
    const SourceCacheLayout cache(cachePath);
    EXPECT_TRUE(std::filesystem::is_directory(cache.gitObject(*commit, true)));
    EXPECT_TRUE(std::filesystem::is_directory(cache.gitObject(*commit, false)));
}

// 测试获取文件类型源码的正常流程
// 场景：提供有效的文件类型source配置，包括url、digest和name
// 预期：成功获取源码并返回ok
TEST_F(SourceFetcherTest, FetchFileSource)
{
    TempDir temporary;
    const auto artifact = temporary.path() / "artifact";
    std::ofstream(artifact) << "file source";
    auto digest = fileHash(artifact);
    ASSERT_TRUE(digest.has_value()) << digest.error().message();

    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.digest = *digest;
    source.name = "repo";

    const auto cached = temporary.path() / "cache/sources/v1/file/sha256" / digest->substr(7);
    std::error_code ec;
    std::filesystem::create_directories(cached.parent_path(), ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::copy_file(artifact, cached, ec);
    ASSERT_FALSE(ec) << ec.message();

    SourceFetcher fetcher(lockedRequest(source, *digest),
                          SourceCacheLayout(temporary.path() / "cache"));
    const auto destination = temporary.path() / "dest";
    ASSERT_TRUE(prepareSourceDirectories(
      temporary.path() / "cache",
      { destination, temporary.path() / "offline", temporary.path() / "corrupt" }));
    auto ret = fetcher.fetch(destination);
    ASSERT_TRUE(ret.has_value()) << ret.error().message();
    EXPECT_EQ(*fileHash(destination / "repo"), *digest);

    struct stat cacheStatus{};
    struct stat buildStatus{};
    ASSERT_EQ(::stat(cached.c_str(), &cacheStatus), 0);
    ASSERT_EQ(::stat((destination / "repo").c_str(), &buildStatus), 0);
    EXPECT_NE(cacheStatus.st_ino, buildStatus.st_ino);

    std::ofstream(destination / "repo", std::ios::trunc) << "modified";
    EXPECT_EQ(*fileHash(cached), *digest);

    SourceFetcher offlineFetcher(lockedRequest(source, *digest),
                                 SourceCacheLayout(temporary.path() / "cache"));
    auto offline = offlineFetcher.fetch(temporary.path() / "offline", true);
    ASSERT_TRUE(offline.has_value()) << offline.error().message();
    EXPECT_EQ(*fileHash(temporary.path() / "offline/repo"), *digest);

    std::ofstream(cached, std::ios::trunc) << "corrupt";
    auto corrupt = offlineFetcher.fetch(temporary.path() / "corrupt", true);
    EXPECT_FALSE(corrupt.has_value());
}

TEST_F(SourceFetcherTest, RejectsInvalidOutputNamesBeforeUsingCache)
{
    TempDir temporary;
    const auto cache = temporary.path() / "cache";
    const auto destination = temporary.path() / "destination";
    ASSERT_TRUE(prepareSourceDirectories(cache, { destination }));
    for (const auto *kind : { "file", "archive", "dsc", "git" }) {
        for (const auto *name : { "", ".", "..", "nested/name", "/absolute" }) {
            SCOPED_TRACE(std::string(kind) + ": " + name);
            api::types::v1::BuilderProjectSource source;
            source.kind = kind;
            source.url = "https://example.com/source";
            source.name = name;
            SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cache));
            auto result = fetcher.fetch(destination, true);
            ASSERT_FALSE(result.has_value());
            EXPECT_NE(result.error().message().find("source name must be a non-empty filename"),
                      std::string::npos)
              << result.error().message();
        }
    }
}

TEST_F(SourceFetcherTest, RejectsUrlWithoutOutputName)
{
    TempDir temporary;
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/dir/";
    SourceFetcher fetcher(GenerateSourceRequest{ source },
                          SourceCacheLayout(temporary.path() / "cache"));
    auto result = fetcher.fetch(temporary.path() / "destination", true);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("source name must be a non-empty filename"),
              std::string::npos)
      << result.error().message();
}

TEST_F(SourceFetcherTest, RejectsNestedNameWithVerifiedCache)
{
    TempDir temporary;
    const auto artifact = temporary.path() / "artifact";
    std::ofstream(artifact) << "file source";
    const auto digest = fileHash(artifact);
    ASSERT_TRUE(digest.has_value()) << digest.error().message();
    const auto cache = temporary.path() / "cache";
    const auto destination = temporary.path() / "destination";
    ASSERT_TRUE(prepareSourceDirectories(cache, { destination }));
    const auto cached = SourceCacheLayout(cache).object("file", *digest);
    std::filesystem::create_directories(cached.parent_path());
    std::filesystem::copy_file(artifact, cached);

    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/source";
    source.name = "nested/name";
    SourceFetcher fetcher(lockedRequest(source, *digest), SourceCacheLayout(cache));
    auto result = fetcher.fetch(destination, true);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(std::filesystem::exists(destination / "nested/name"));
}

TEST_F(SourceFetcherTest, MaterializesValidPosixOutputNames)
{
    TempDir temporary;
    const auto artifact = temporary.path() / "artifact";
    std::ofstream(artifact) << "file source";
    const auto digest = fileHash(artifact);
    ASSERT_TRUE(digest.has_value()) << digest.error().message();
    const auto cache = temporary.path() / "cache";
    const auto destination = temporary.path() / "destination";
    ASSERT_TRUE(prepareSourceDirectories(cache, { destination }));
    const auto cached = SourceCacheLayout(cache).object("file", *digest);
    std::filesystem::create_directories(cached.parent_path());
    std::filesystem::copy_file(artifact, cached);
    for (const auto *name : { "valid name.txt", "valid\\name.txt" }) {
        api::types::v1::BuilderProjectSource source;
        source.kind = "file";
        source.url = "https://example.com/source";
        source.name = name;
        SourceFetcher fetcher(lockedRequest(source, *digest), SourceCacheLayout(cache));
        auto result = fetcher.fetch(destination, true);
        ASSERT_TRUE(result.has_value()) << result.error().message();
        EXPECT_EQ(*fileHash(destination / name), *digest);
    }
}

TEST_F(SourceFetcherTest, MaterializesCachedArchive)
{
    TempDir temporary;

    utils::Cmd tar("tar");
    if (!tar.exists()) {
        GTEST_SKIP() << "tar is not available";
    }
    const auto content = temporary.path() / "content";
    std::error_code ec;
    std::filesystem::create_directories(content, ec);
    ASSERT_FALSE(ec) << ec.message();
    std::ofstream(content / "source.txt") << "source";
    const auto artifact = temporary.path() / "source.tar";
    auto archived = tar.exec({ "-cf", artifact.string(), "-C", content.string(), "." });
    ASSERT_TRUE(archived.has_value()) << archived.error().message();
    auto digest = fileHash(artifact);
    ASSERT_TRUE(digest.has_value()) << digest.error().message();

    api::types::v1::BuilderProjectSource source;
    source.kind = "archive";
    source.url = "https://example.com/source.tar.gz";
    source.digest = *digest;
    source.name = "source";

    const auto cached = temporary.path() / "cache/sources/v1/archive/sha256" / digest->substr(7);
    std::filesystem::create_directories(cached.parent_path(), ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::copy_file(artifact, cached, ec);
    ASSERT_FALSE(ec) << ec.message();

    SourceFetcher fetcher(lockedRequest(source, *digest),
                          SourceCacheLayout(temporary.path() / "cache"));
    ASSERT_TRUE(
      prepareSourceDirectories(temporary.path() / "cache", { temporary.path() / "destination" }));
    auto fetched = fetcher.fetch(temporary.path() / "destination", true);
    ASSERT_TRUE(fetched.has_value()) << fetched.error().message();
    EXPECT_EQ(fetched->hash, *digest);
    EXPECT_TRUE(
      std::filesystem::is_regular_file(temporary.path() / "destination/source/source.txt", ec));
    EXPECT_FALSE(ec);
}

// 测试无效的source类型
// 场景：提供无效的source.kind值
// 预期：返回错误结果，错误码为-1
TEST_F(SourceFetcherTest, FetchInvalidSourceKind)
{
    TempDir temporary;
    api::types::v1::BuilderProjectSource source;
    source.kind = "invalid";
    source.url = "https://example.com/repo.git";
    source.name = "repo";

    const auto cacheDir = temporary.path() / "cache";
    const auto destination = temporary.path() / "sources";
    ASSERT_TRUE(prepareSourceDirectories(cacheDir, { destination }));
    SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cacheDir));
    auto ret = fetcher.fetch(destination);
    ASSERT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -1);
    EXPECT_NE(ret.error().message().find("unknown source kind"), std::string::npos);
}

// 测试缺失必填字段的情况
// 场景：缺少url和name字段
// 预期：返回错误结果，错误码为-1
TEST_F(SourceFetcherTest, FetchMissingRequiredField)
{
    TempDir temporary;
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    // 缺少 url 和 name
    source.commit = "abcdef123456";

    const auto cacheDir = temporary.path() / "cache";
    const auto destination = temporary.path() / "sources";
    ASSERT_TRUE(prepareSourceDirectories(cacheDir, { destination }));
    SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cacheDir));
    auto ret = fetcher.fetch(destination);
    ASSERT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -1);
    EXPECT_NE(ret.error().message().find("source URL is missing"), std::string::npos);
}

// 测试未设置git commit
// 预期：在调用git脚本前返回错误结果
TEST_F(SourceFetcherTest, FetchInvalidGitCommit)
{
    LINGLONG_TRACE("FetchInvalidGitCommit");

    TempDir temporary;
    api::types::v1::BuilderProjectSource source;
    source.kind = "git";
    source.url = "https://example.com/repo.git";
    source.name = "repo";

    const auto cacheDir = temporary.path() / "cache";
    const auto destination = temporary.path() / "sources";
    ASSERT_TRUE(prepareSourceDirectories(cacheDir, { destination }));
    {
        SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cacheDir));
        auto ret = fetcher.fetch(destination);
        EXPECT_FALSE(ret.has_value());
    }
}

// 测试无效的文件digest
// 场景：设置无效的digest值
// 预期：在调用下载脚本前返回错误结果
TEST_F(SourceFetcherTest, FetchInvalidFileDigest)
{
    LINGLONG_TRACE("FetchInvalidFileDigest");

    TempDir temporary;
    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.name = "repo";
    const auto cacheDir = temporary.path() / "cache";
    const auto destination = temporary.path() / "sources";
    ASSERT_TRUE(prepareSourceDirectories(cacheDir, { destination }));
    source.digest = "invalid_digest";
    SourceFetcher fetcher(GenerateSourceRequest{ source }, SourceCacheLayout(cacheDir));
    auto ret = fetcher.fetch(destination);
    EXPECT_FALSE(ret.has_value());
    EXPECT_EQ(ret.error().code(), -1);
}

// 测试未设置name字段的情况
// 场景：仅设置url和digest字段
// 预期：仍能成功获取源码
TEST_F(SourceFetcherTest, FetchNoSetName)
{
    TempDir temporary;
    const auto artifact = temporary.path() / "artifact";
    std::ofstream(artifact) << "file source";
    auto digest = fileHash(artifact);
    ASSERT_TRUE(digest.has_value()) << digest.error().message();

    api::types::v1::BuilderProjectSource source;
    source.kind = "file";
    source.url = "https://example.com/repo.git";
    source.digest = *digest;

    const auto cached = temporary.path() / "cache/sources/v1/file/sha256" / digest->substr(7);
    std::error_code ec;
    std::filesystem::create_directories(cached.parent_path(), ec);
    ASSERT_FALSE(ec) << ec.message();
    std::filesystem::copy_file(artifact, cached, ec);
    ASSERT_FALSE(ec) << ec.message();

    SourceFetcher fetcher(lockedRequest(source, *digest),
                          SourceCacheLayout(temporary.path() / "cache"));
    ASSERT_TRUE(
      prepareSourceDirectories(temporary.path() / "cache", { temporary.path() / "dest" }));
    auto ret = fetcher.fetch(temporary.path() / "dest");
    EXPECT_TRUE(ret.has_value());
}

} // namespace linglong::builder
