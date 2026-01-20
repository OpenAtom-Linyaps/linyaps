// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
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

pid_t FileLock::pid() const noexcept
{
    return pid_;
}

utils::error::Result<FileLock> FileLock::create(std::filesystem::path path,
                                                bool create_if_missing) noexcept
{
    LINGLONG_TRACE("create file lock");

    auto cur_pid = ::getpid();
    auto &[global_pid, locked_paths] = process_locked_paths;
    if (global_pid != cur_pid) {
        global_pid = cur_pid;
        locked_paths.clear();
    }

    std::error_code ec;
    auto abs_path = std::filesystem::absolute(path, ec);
    if (ec) {
        return LINGLONG_ERR(fmt::format("canonicalize path {} error: {}", path, ec.message()));
    }

    if (locked_paths.find(abs_path) != locked_paths.end()) {
        return LINGLONG_ERR(fmt::format("process already holds a lock on file {}", abs_path));
    }

    unsigned int flags = O_RDWR | O_CLOEXEC | O_NOFOLLOW;
    if (create_if_missing) {
        flags |= O_CREAT;
    }

    auto fd = ::open(abs_path.c_str(), flags, default_file_mode);
    if (fd < 0) {
        return LINGLONG_ERR(fmt::format("open file failed: {}", common::error::errorString(errno)));
    }

    locked_paths[abs_path] = true;
    FileLock lock(fd, std::move(abs_path));

    return lock;
}

FileLock::~FileLock() noexcept
{
    if (isLocked()) {
        auto ret = unlock();
        if (!ret) {
            LogW("unlock file failed: {}", ret.error());
        }
    }

    if (fd > 0 && ::close(fd) < 0) {
        LogW("close file failed: {}", common::error::errorString(errno));
    }

    fd = -1;
    auto it = process_locked_paths.second.find(path);
    if (it != process_locked_paths.second.end()) {
        process_locked_paths.second.erase(it);
    }
}

FileLock::FileLock(int fd, std::filesystem::path path) noexcept
    : fd(fd)
    , path(std::move(path))
{
}

FileLock::FileLock(FileLock &&other) noexcept
    : type_(other.type_)
    , fd(other.fd)
    , path(std::move(other.path))
{
    if (other.pid_ != pid()) {
        LogF("move lock to different process");
    }

    locked.store(other.locked.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.locked.store(false, std::memory_order_relaxed);
    other.fd = -1;
}

FileLock &FileLock::operator=(FileLock &&other) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (other.pid_ != pid()) {
        LogF("move lock to different process");
    }

    fd = other.fd;
    path = std::move(other.path);
    type_ = other.type_;

    locked.store(other.locked.load(std::memory_order_relaxed), std::memory_order_relaxed);
    other.locked.store(false, std::memory_order_relaxed);
    other.fd = -1;

    return *this;
}

utils::error::Result<void> FileLock::lockCheck() const noexcept
{
    LINGLONG_TRACE("validating file lock");

    if (pid_ != ::getpid()) {
        return LINGLONG_ERR("cannot operate on lock from forked process");
    }

    return LINGLONG_OK;
}

utils::error::Result<void> FileLock::lock(LockType type) noexcept
{
    LINGLONG_TRACE("lock file");

    auto ret = lockCheck();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (isLocked()) {
        if (type == type_) {
            return LINGLONG_OK;
        }

        return LINGLONG_ERR(
          fmt::format("use relock to change lock type from {} to {}", type_, type));
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

    auto ret = lockCheck();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    if (isLocked()) {
        if (type == type_) {
            return LINGLONG_OK;
        }

        return LINGLONG_ERR(
          fmt::format("use relock to change lock type from {} to {}", type_, type));
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

    auto ret = lockCheck();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

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

utils::error::Result<void> FileLock::relock(LockType new_type) noexcept
{
    LINGLONG_TRACE("upgrade lock type");

    if (type_ == new_type) {
        return LINGLONG_OK;
    }

    auto ret = lockCheck();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    struct flock fl{};
    fl.l_type = (new_type == LockType::Write) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    while (true) {
        if (::fcntl(fd, F_SETLKW, &fl) == 0) {
            type_ = new_type;
            return LINGLONG_OK;
        }

        if (errno == EINTR) {
            continue;
        }

        return LINGLONG_ERR(
          fmt::format("failed to relock file {}: {}", path, common::error::errorString(errno)));
    }
}

bool FileLock::isLocked() const noexcept
{
    return locked.load(std::memory_order_relaxed) && pid() == ::getpid();
}

} // namespace linglong::utils::filelock
