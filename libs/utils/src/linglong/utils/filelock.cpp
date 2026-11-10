// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/filelock.h"

#include "linglong/common/constants.h"
#include "linglong/common/error.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#include <filesystem>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong::utils::filelock {

utils::error::Result<FileLock> FileLock::create(std::filesystem::path path,
                                                LockType type,
                                                bool create_if_missing) noexcept
{
    LINGLONG_TRACE("create file lock");

    std::lock_guard<std::mutex> guard(process_paths_mutex);
    auto &locked_paths = process_locked_paths;

    std::error_code ec;
    auto abs_path = std::filesystem::absolute(path, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("canonicalize path {} error: {}", path, ec.message()));
    }

    if (locked_paths.find(abs_path) != locked_paths.end()) {
        return LINGLONG_ERR(fmt::format("process already holds a lock on file {}", abs_path));
    }

    unsigned int flags = O_CLOEXEC | O_NOFOLLOW;
    switch (type) {
    case LockType::Read:
        flags |= O_RDONLY;
        break;
    case LockType::Write:
        flags |= O_WRONLY;
        break;
    case LockType::ReadWrite:
        flags |= O_RDWR;
        break;
    }

    if (create_if_missing) {
        LogD("create lock file {} if missing", abs_path);
        flags |= O_CREAT;
    }

    auto fd = ::open(abs_path.c_str(), flags, common::default_file_mode);
    if (fd < 0) {
        return LINGLONG_ERR(fmt::format("failed to open lock file {} :{}",
                                        abs_path,
                                        common::error::errorString(errno)));
    }

    locked_paths[abs_path] = true;
    FileLock lock(fd, std::move(abs_path), type);

    return lock;
}

FileLock::~FileLock() noexcept
{
    release();
}

void FileLock::release() noexcept
{
    // A moved-from local may be destroyed while create() still holds the registry mutex.
    if (fd < 0) {
        return;
    }

    if (isLocked()) {
        auto ret = unlock();
        if (!ret) {
            LogW("unlock file failed: {}", ret.error());
        }
    }

    if (::close(fd) < 0) {
        LogW("close file failed: {}", common::error::errorString(errno));
    }

    fd = -1;
    locked.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> guard(process_paths_mutex);
    auto it = process_locked_paths.find(path);
    if (it != process_locked_paths.end()) {
        process_locked_paths.erase(it);
    }
    path.clear();
}

FileLock::FileLock(int fd, std::filesystem::path path, LockType type) noexcept
    : type_(type)
    , fd(fd)
    , path(std::move(path))
{
}

FileLock::FileLock(FileLock &&other) noexcept
    : type_(other.type_)
    , fd(other.fd)
    , path(std::move(other.path))
{
    locked.store(other.locked.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.locked.store(false, std::memory_order_relaxed);
    other.fd = -1;
}

FileLock &FileLock::operator=(FileLock &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    release();

    fd = other.fd;
    path = std::move(other.path);
    type_ = other.type_;

    locked.store(other.locked.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.locked.store(false, std::memory_order_relaxed);
    other.fd = -1;

    return *this;
}

utils::error::Result<void> FileLock::lock(LockType type) noexcept
{
    LINGLONG_TRACE("lock file");

    auto isCompatible = compatibleWith(type);
    if (isLocked()) {
        if (isCompatible) {
            return LINGLONG_OK;
        }

        return LINGLONG_ERR(
          fmt::format("failed to lock with different type from {} to {}", type_, type));
    }

    if (!isCompatible) {
        return LINGLONG_ERR(
          fmt::format("try to lock with incompatible type: current {}, request {}", type_, type));
    }

    struct flock fl{};
    fl.l_type = (type == LockType::Write) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    while (true) {
        if (::fcntl(fd, F_SETLKW, &fl) == 0) {
            type_ = type;
            locked.store(true, std::memory_order_relaxed);
            return LINGLONG_OK;
        }

        if (errno == EINTR) {
            continue;
        }

        return LINGLONG_ERR(
          fmt::format("failed to lock file {}: {}", path, common::error::errorString(errno)));
    }
}

utils::error::Result<bool> FileLock::tryLock(LockType type) noexcept
{
    LINGLONG_TRACE("try lock file");

    auto isCompatible = compatibleWith(type);
    if (isLocked()) {
        if (isCompatible) {
            return true;
        }

        return LINGLONG_ERR(
          fmt::format("use relock to change lock type from {} to {}", type_, type));
    }

    if (!isCompatible) {
        return LINGLONG_ERR(
          fmt::format("try to lock with incompatible type: current {}, request {}", type_, type));
    }

    struct flock fl{};
    fl.l_type = (type == LockType::Write) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    while (true) {
        if (::fcntl(fd, F_SETLK, &fl) == 0) {
            type_ = type;
            locked.store(true, std::memory_order_relaxed);
            return true;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EACCES || errno == EAGAIN) {
            return false;
        }

        return LINGLONG_ERR(
          fmt::format("failed to lock file {}: {}", path, common::error::errorString(errno)));
    }
}

utils::error::Result<void> FileLock::unlock() noexcept
{
    LINGLONG_TRACE("unlock file");

    if (!isLocked()) {
        return LINGLONG_OK;
    }

    struct flock fl{};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    while (true) {
        if (::fcntl(fd, F_SETLK, &fl) == 0) {
            locked.store(false, std::memory_order_relaxed);
            return LINGLONG_OK;
        }
        if (errno == EINTR) {
            continue;
        }
        return LINGLONG_ERR(
          fmt::format("failed to unlock file {}: {}", path, common::error::errorString(errno)));
    }
}

bool FileLock::isLocked() const noexcept
{
    return locked.load(std::memory_order_relaxed);
}

} // namespace linglong::utils::filelock
