/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../../common/tempdir.h"
#include "linglong/package_manager/action.h"
#include "linglong/package_manager/package_manager.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/runtime/container_builder.h"
#include "linglong/utils/finally/finally.h"
#include "ocppi/cli/crun/Crun.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace linglong::runtime {
class ContainerBuilder;
}

namespace linglong::service {
class PackageTask;
}

using namespace linglong;
using ::testing::_;
using ::testing::Return;

namespace {

class MockRepo : public repo::OSTreeRepo
{
public:
    MockRepo(const std::filesystem::path &path)
        : repo::OSTreeRepo(
            path, api::types::v1::RepoConfigV2{ .defaultRepo = "", .repos = {}, .version = 2 })
    {
    }

    MOCK_METHOD(utils::error::Result<std::vector<api::types::v1::RepositoryCacheLayersItem>>,
                listLocalBy,
                (const repo::repoCacheQuery &query),
                (override, const, noexcept));
};

class MockAction : public service::Action
{
public:
    MockAction(service::PackageManager &pm,
               repo::OSTreeRepo &repo,
               api::types::v1::CommonOptions opts)
        : service::Action(pm, repo, opts)
    {
    }

    utils::error::Result<service::ActionOperation>
    testActionOperation(const api::types::v1::PackageInfoV2 &target, bool extraModuleOnly)
    {
        return getActionOperation(target, extraModuleOnly);
    }

    utils::error::Result<void> prepare() override { return utils::error::Result<void>(); }

    utils::error::Result<void> doAction([[maybe_unused]] service::PackageTask &task) override
    {
        return utils::error::Result<void>();
    }

    std::string getTaskName() const override { return "mock_action"; }
};

class TestPackageManager : public service::PackageManager
{
public:
    using service::PackageManager::getLockFd;
    using service::PackageManager::lockRepo;
    using service::PackageManager::PackageManager;
    using service::PackageManager::setRepoLockPath;
    using service::PackageManager::unlockRepo;

    void injectLockFdForTest(int fd) noexcept
    {
        const std::lock_guard<std::mutex> lock(m_repoLockMutex);
        lockFd = fd;
    }
};

class ActionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        tempDir = std::make_unique<TempDir>();
        auto repoOwner = std::make_unique<MockRepo>(tempDir->path());
        repo = repoOwner.get();
        cli = ocppi::cli::crun::Crun::New(tempDir->path()).value();
        auto containerBuilderOwner = std::make_unique<runtime::ContainerBuilder>(*cli);
        containerBuilder = containerBuilderOwner.get();
        pm = std::make_unique<TestPackageManager>(std::move(repoOwner),
                                                  std::move(containerBuilderOwner),
                                                  nullptr);
        mockAction = std::make_unique<MockAction>(*pm, *repo, api::types::v1::CommonOptions());
    }

    void TearDown() override
    {
        mockAction.reset();
        pm.reset();
        cli.reset();
        tempDir.reset();
    }

    std::unique_ptr<TempDir> tempDir;
    std::unique_ptr<ocppi::cli::crun::Crun> cli;
    runtime::ContainerBuilder *containerBuilder{ nullptr };
    MockRepo *repo{ nullptr };
    std::unique_ptr<TestPackageManager> pm;
    std::unique_ptr<MockAction> mockAction;
};

TEST_F(ActionTest, InstallNewApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{}));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, OverwriteApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
          .info = packageInfo,
        } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Overwrite);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, UpgradeApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "2.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "app",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "1.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Upgrade);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/2.0.0/x86_64");
}

TEST_F(ActionTest, DowngradeApp)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "app",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "app",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "2.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Downgrade);
    EXPECT_EQ(result->kind, "app");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/2.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, InstallLowVersionRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "runtime",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "2.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, InstallHighVersionRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "2.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_))
      .WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{ .info = {
                                                     .arch = std::vector<std::string>{ "x86_64" },
                                                     .channel = "main",
                                                     .id = "id1",
                                                     .kind = "runtime",
                                                     .packageInfoV2Module = "binary",
                                                     .version = "1.0.0",
                                                   } } }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Install);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef, std::nullopt);
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/2.0.0/x86_64");
}

TEST_F(ActionTest, InstallSameRuntime)
{
    api::types::v1::PackageInfoV2 packageInfo = {
        .arch = std::vector<std::string>{ "x86_64" },
        .channel = "main",
        .id = "id1",
        .kind = "runtime",
        .packageInfoV2Module = "binary",
        .version = "1.0.0",
    };

    EXPECT_CALL(*repo, listLocalBy(_)).WillOnce(Return(std::vector<api::types::v1::RepositoryCacheLayersItem>{
        api::types::v1::RepositoryCacheLayersItem{
            .info = {
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "id1",
                .kind = "runtime",
                .packageInfoV2Module = "binary",
                .version = "2.0.0",
            }
        },
        api::types::v1::RepositoryCacheLayersItem{
            .info = {
                .arch = std::vector<std::string>{ "x86_64" },
                .channel = "main",
                .id = "id1",
                .kind = "runtime",
                .packageInfoV2Module = "binary",
                .version = "1.0.0",
            }
        }
    }));

    auto result = mockAction->testActionOperation(packageInfo, false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->operation, service::ActionOperation::Policy::Overwrite);
    EXPECT_EQ(result->kind, "runtime");
    EXPECT_EQ(result->oldRef->toString(), "main:id1/1.0.0/x86_64");
    EXPECT_EQ(result->newRef->reference.toString(), "main:id1/1.0.0/x86_64");
}

TEST_F(ActionTest, PackageManagerRepoLockLifecycle)
{
    const auto lockFilePath = tempDir->path() / "test_repo_lifecycle.lock";
    {
        std::ofstream ofs(lockFilePath);
        ASSERT_TRUE(ofs.is_open());
    }

    pm->setRepoLockPath(lockFilePath);

    // Initial state: not locked, unlockRepo returns success cleanly.
    EXPECT_EQ(pm->getLockFd(), -1);
    EXPECT_TRUE(pm->unlockRepo());
    EXPECT_EQ(pm->getLockFd(), -1);

    // Acquire repo lock
    auto lockRet = pm->lockRepo();
    ASSERT_TRUE(lockRet) << lockRet.error().message();
    EXPECT_NE(pm->getLockFd(), -1);

    // Verify FD_CLOEXEC flag is set on the opened descriptor
    int flags = ::fcntl(pm->getLockFd(), F_GETFD);
    ASSERT_NE(flags, -1);
    EXPECT_TRUE((flags & FD_CLOEXEC) != 0);

    // Re-entrant lock attempt must be rejected
    auto secondLock = pm->lockRepo();
    EXPECT_FALSE(secondLock);

    // Unlocking releases the lock and resets lockFd to -1
    EXPECT_TRUE(pm->unlockRepo());
    EXPECT_EQ(pm->getLockFd(), -1);

    // Double unlock is safe and idempotent
    EXPECT_TRUE(pm->unlockRepo());
    EXPECT_EQ(pm->getLockFd(), -1);
}

TEST_F(ActionTest, PackageManagerRepoLockConcurrentReentrancyProtection)
{
    const auto lockFilePath = tempDir->path() / "test_repo_concurrent.lock";
    {
        std::ofstream ofs(lockFilePath);
        ASSERT_TRUE(ofs.is_open());
    }

    pm->setRepoLockPath(lockFilePath);

    constexpr int kThreadCount = 8;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);

    std::atomic<bool> startFlag{ false };
    std::atomic<int> readyCount{ 0 };
    std::vector<utils::error::Result<void>> results(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&, i] {
            readyCount.fetch_add(1, std::memory_order_release);
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            results[i] = pm->lockRepo();
        });
    }

    // Wait until all threads are ready to race
    while (readyCount.load(std::memory_order_acquire) < kThreadCount) {
        std::this_thread::yield();
    }

    // Release all threads simultaneously
    startFlag.store(true, std::memory_order_release);

    for (auto &t : threads) {
        t.join();
    }

    // Exactly one thread must succeed in locking
    int successCount = 0;
    int failureCount = 0;
    for (const auto &res : results) {
        if (res.has_value()) {
            ++successCount;
        } else {
            ++failureCount;
            EXPECT_THAT(res.error().message(), ::testing::HasSubstr("already locked"));
        }
    }

    EXPECT_EQ(successCount, 1);
    EXPECT_EQ(failureCount, kThreadCount - 1);
    EXPECT_NE(pm->getLockFd(), -1);

    // Test concurrent unlocking: multiple threads calling unlockRepo concurrently
    std::atomic<bool> unlockStartFlag{ false };
    std::atomic<int> unlockReadyCount{ 0 };
    std::vector<utils::error::Result<void>> unlockResults(kThreadCount);
    std::vector<std::thread> unlockThreads;
    unlockThreads.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i) {
        unlockThreads.emplace_back([&, i] {
            unlockReadyCount.fetch_add(1, std::memory_order_release);
            while (!unlockStartFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            unlockResults[i] = pm->unlockRepo();
        });
    }

    while (unlockReadyCount.load(std::memory_order_acquire) < kThreadCount) {
        std::this_thread::yield();
    }
    unlockStartFlag.store(true, std::memory_order_release);

    for (auto &t : unlockThreads) {
        t.join();
    }

    for (const auto &res : unlockResults) {
        EXPECT_TRUE(res);
    }
    EXPECT_EQ(pm->getLockFd(), -1);

    // After unlocking, subsequent lock and unlock work cleanly
    auto finalLock = pm->lockRepo();
    EXPECT_TRUE(finalLock);
    EXPECT_NE(pm->getLockFd(), -1);
    EXPECT_TRUE(pm->unlockRepo());
    EXPECT_EQ(pm->getLockFd(), -1);
}

TEST_F(ActionTest, PackageManagerRepoLockContentionNoLeak)
{
    const auto lockFilePath = tempDir->path() / "test_repo_contention.lock";
    {
        std::ofstream ofs(lockFilePath);
        ASSERT_TRUE(ofs.is_open());
    }

    pm->setRepoLockPath(lockFilePath);

    int childReadyPipe[2]{ -1, -1 };
    int parentDonePipe[2]{ -1, -1 };
    pid_t pid = -1;

    auto closeFd = [](int &fd) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    };

    // RAII cleanup guard established BEFORE pipe creation and fork
    auto cleanup = utils::finally::finally([&] {
        closeFd(childReadyPipe[0]);
        closeFd(childReadyPipe[1]);
        closeFd(parentDonePipe[0]);
        closeFd(parentDonePipe[1]);
        if (pid > 0) {
            ::kill(pid, SIGKILL);
            int status = 0;
            while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) { }
            pid = -1;
        }
    });

    ASSERT_EQ(::pipe(childReadyPipe), 0);
    ASSERT_EQ(::pipe(parentDonePipe), 0);

    pid = ::fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // Child process
        closeFd(childReadyPipe[0]);
        closeFd(parentDonePipe[1]);

        int childFd = ::open(lockFilePath.c_str(), O_WRONLY | O_CLOEXEC);
        if (childFd == -1) {
            ::_exit(1);
        }

        struct flock locker{ .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
        if (::fcntl(childFd, F_SETLK, &locker) == -1) {
            ::close(childFd);
            ::_exit(2);
        }

        char ready = 'R';
        ssize_t written = 0;
        do {
            written = ::write(childReadyPipe[1], &ready, sizeof(ready));
        } while (written < 0 && errno == EINTR);

        closeFd(childReadyPipe[1]);
        if (written != sizeof(ready)) {
            ::close(childFd);
            ::_exit(3);
        }

        char done = 0;
        ssize_t nread = 0;
        do {
            nread = ::read(parentDonePipe[0], &done, sizeof(done));
        } while (nread < 0 && errno == EINTR);

        closeFd(parentDonePipe[0]);
        ::close(childFd);
        ::_exit(0);
    }

    // Parent process
    closeFd(childReadyPipe[1]);
    closeFd(parentDonePipe[0]);

    // Wait until child has successfully acquired the lock
    char ready = 0;
    ssize_t nread = 0;
    do {
        nread = ::read(childReadyPipe[0], &ready, sizeof(ready));
    } while (nread < 0 && errno == EINTR);
    ASSERT_EQ(nread, 1);
    EXPECT_EQ(ready, 'R');
    closeFd(childReadyPipe[0]);

    // On Linux test environments, /proc/self/fd is available to verify file descriptors
    std::error_code ec;
    ASSERT_TRUE(std::filesystem::exists("/proc/self/fd", ec));

    auto countOpenFds = [](size_t &outCount) -> bool {
        std::error_code ec;
        std::filesystem::directory_iterator it("/proc/self/fd", ec);
        if (ec) {
            return false;
        }
        size_t count = 0;
        const std::filesystem::directory_iterator endIt;
        while (it != endIt) {
            ++count;
            it.increment(ec);
            if (ec) {
                return false;
            }
        }
        outCount = count;
        return true;
    };

    size_t fdsBefore = 0;
    ASSERT_TRUE(countOpenFds(fdsBefore));

    // With the child holding the lock, lockRepo() must fail and close descriptor immediately
    for (int i = 0; i < 5; ++i) {
        auto ret = pm->lockRepo();
        EXPECT_FALSE(ret);
        EXPECT_EQ(pm->getLockFd(), -1);
    }

    size_t fdsAfter = 0;
    ASSERT_TRUE(countOpenFds(fdsAfter));
    EXPECT_EQ(fdsBefore, fdsAfter);

    // Signal child to release lock and exit
    char done = 'D';
    ssize_t written = 0;
    do {
        written = ::write(parentDonePipe[1], &done, sizeof(done));
    } while (written < 0 && errno == EINTR);
    EXPECT_EQ(written, 1);
    closeFd(parentDonePipe[1]);

    int status = 0;
    pid_t waitedPid = -1;
    do {
        waitedPid = ::waitpid(pid, &status, 0);
    } while (waitedPid < 0 && errno == EINTR);

    ASSERT_EQ(waitedPid, pid);

    // waitpid has already successfully reaped the child process, clear pid immediately
    // so cleanup guard does not try to kill a recycled PID if subsequent status assertions fail.
    pid = -1;

    ASSERT_TRUE(WIFEXITED(status));
    ASSERT_EQ(WEXITSTATUS(status), 0);

    // After child exits, repo lock is free and lockRepo() should succeed
    auto lockRet = pm->lockRepo();
    EXPECT_TRUE(lockRet);
    EXPECT_NE(pm->getLockFd(), -1);
    EXPECT_TRUE(pm->unlockRepo());
    EXPECT_EQ(pm->getLockFd(), -1);
}

TEST_F(ActionTest, PackageManagerRepoUnlockFailureCleansUpAndResets)
{
    // Create a pipe: valid open descriptor, but fcntl(F_SETLK) fails with EINVAL on FIFOs/pipes
    int pipeFds[2]{ -1, -1 };
    ASSERT_EQ(::pipe(pipeFds), 0);
    ::close(pipeFds[1]); // Close write end

    int testFd = pipeFds[0];
    pm->injectLockFdForTest(testFd);

    // unlockRepo must fail because fcntl(F_UNLCK) is not supported on a pipe
    auto unlockRet = pm->unlockRepo();
    EXPECT_FALSE(unlockRet);

    // lockFd MUST be reset to -1 despite the fcntl failure
    EXPECT_EQ(pm->getLockFd(), -1);

    // testFd MUST be closed unconditionally by unlockRepo()
    EXPECT_EQ(::close(testFd), -1);
    EXPECT_EQ(errno, EBADF);

    // Subsequent unlock call returns success cleanly
    EXPECT_TRUE(pm->unlockRepo());
}

} // namespace
