// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/common/dir.h"

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace linglong::test {

// The package manager resolves its staging and lock paths from compile-time
// constants, so unit tests cannot inject a temporary root.  These guards only
// permit access to those paths when the test process is running in a disposable
// container (or an explicitly isolated runtime), and they restore any state
// that existed before the test.
inline bool isIsolatedRuntime() noexcept
{
    std::error_code ec;
    if (std::filesystem::exists("/.dockerenv", ec)) {
        return true;
    }
    if (std::filesystem::exists("/run/.containerenv", ec)) {
        return true;
    }

    const char *container = std::getenv("container");
    if (container != nullptr && container[0] != '\0') {
        return true;
    }

    const char *optIn = std::getenv("LINGLONG_TEST_ISOLATED_RUNTIME");
    return optIn != nullptr && std::string{ optIn } == "1";
}

inline std::string isolatedRuntimeRequirement()
{
    return "test requires a disposable container or LINGLONG_TEST_ISOLATED_RUNTIME=1 "
           "to avoid touching the host package-manager state";
}

class ScopedStagingDir
{
public:
    ScopedStagingDir()
    {
        if (!isIsolatedRuntime()) {
            failure = isolatedRuntimeRequirement();
            return;
        }

        path = common::dir::getStagingDir();
        const auto parent = path.parent_path();
        std::error_code ec;
        const bool parentExisted = std::filesystem::exists(parent, ec);
        if (ec) {
            failure = "failed to inspect staging parent " + parent.string() + ": " + ec.message();
            return;
        }

        if (!parentExisted) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                failure =
                  "failed to create staging parent " + parent.string() + ": " + ec.message();
                return;
            }
            parentCreated = true;
        }

        if (std::filesystem::exists(path, ec)) {
            if (ec) {
                failure =
                  "failed to inspect staging directory " + path.string() + ": " + ec.message();
                return;
            }

            backup = parent / (".ll-tests-staging-backup-" + std::to_string(::getpid()));
            if (std::filesystem::exists(backup, ec)) {
                failure = "staging backup path already exists: " + backup.string();
                return;
            }

            std::filesystem::rename(path, backup, ec);
            if (ec) {
                failure = "failed to move existing staging directory aside: " + ec.message();
                return;
            }
            hasBackup = true;
        }

        std::filesystem::create_directories(path, ec);
        if (ec) {
            restore();
            failure =
              "failed to create isolated staging directory " + path.string() + ": " + ec.message();
            return;
        }
        created = true;
    }

    ScopedStagingDir(const ScopedStagingDir &) = delete;
    ScopedStagingDir &operator=(const ScopedStagingDir &) = delete;

    ~ScopedStagingDir() { restore(); }

    [[nodiscard]] bool ok() const noexcept { return failure.empty(); }

    [[nodiscard]] const std::string &error() const noexcept { return failure; }

    [[nodiscard]] const std::filesystem::path &stagingPath() const noexcept { return path; }

private:
    void restore() noexcept
    {
        std::error_code ec;
        if (created) {
            std::filesystem::remove_all(path, ec);
            created = false;
        }
        if (hasBackup) {
            std::filesystem::rename(backup, path, ec);
            hasBackup = false;
        }
        if (parentCreated) {
            std::filesystem::remove(path.parent_path(), ec);
            parentCreated = false;
        }
    }

    std::filesystem::path path;
    std::filesystem::path backup;
    bool created{ false };
    bool hasBackup{ false };
    bool parentCreated{ false };
    std::string failure;
};

class ScopedRepoLockFile
{
public:
    ScopedRepoLockFile()
    {
        if (!isIsolatedRuntime()) {
            failure = isolatedRuntimeRequirement();
            return;
        }

        path = common::dir::repoLockPath;
        const auto parent = path.parent_path();
        std::error_code ec;
        const bool parentExisted = std::filesystem::exists(parent, ec);
        if (ec) {
            failure = "failed to inspect repo lock parent " + parent.string() + ": " + ec.message();
            return;
        }
        if (!parentExisted) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                failure =
                  "failed to create repo lock parent " + parent.string() + ": " + ec.message();
                return;
            }
            parentCreated = true;
        }

        const auto probe = parent / (".ll-tests-lock-probe-" + std::to_string(::getpid()));
        int probeFd = ::open(probe.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (probeFd == -1) {
            failure = "repo lock directory " + parent.string() + " is not writable";
            return;
        }
        ::close(probeFd);
        std::filesystem::remove(probe, ec);

        int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd >= 0) {
            ::close(fd);
            created = true;
            return;
        }

        if (errno != EEXIST) {
            failure = "failed to create repo lock file " + path.string();
            return;
        }

        fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
        if (fd == -1) {
            failure = "existing repo lock file is not writable: " + path.string();
            return;
        }

        struct flock probeLock{ .l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0 };
        if (::fcntl(fd, F_GETLK, &probeLock) == -1) {
            ::close(fd);
            failure = "failed to inspect existing repo lock: " + path.string();
            return;
        }
        ::close(fd);
        if (probeLock.l_type != F_UNLCK) {
            failure = "repo lock is already held by another process";
        }
    }

    ScopedRepoLockFile(const ScopedRepoLockFile &) = delete;
    ScopedRepoLockFile &operator=(const ScopedRepoLockFile &) = delete;

    ~ScopedRepoLockFile()
    {
        if (created) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
        }
        if (parentCreated) {
            std::error_code ec;
            std::filesystem::remove(path.parent_path(), ec);
        }
    }

    [[nodiscard]] bool ok() const noexcept { return failure.empty(); }

    [[nodiscard]] const std::string &error() const noexcept { return failure; }

private:
    std::filesystem::path path;
    bool created{ false };
    bool parentCreated{ false };
    std::string failure;
};

} // namespace linglong::test
