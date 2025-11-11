// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unordered_map>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace linglong::utils::filelock {

enum class LockType : uint8_t {
    Read,
    Write,
};

class FileLock
{
public:
    static utils::error::Result<FileLock> create(std::filesystem::path path,
                                                 bool create_if_missing = true) noexcept;

    ~FileLock() noexcept;

    FileLock(const FileLock &) = delete;
    FileLock &operator=(const FileLock &) = delete;
    FileLock(FileLock &&) noexcept;
    FileLock &operator=(FileLock &&) noexcept;

    utils::error::Result<void> lock(LockType type) noexcept;

    utils::error::Result<bool> tryLock(LockType type) noexcept;

    template <typename Rep, typename Period>
    utils::error::Result<bool>
    tryLockFor(LockType type,
               const std::chrono::duration<Rep, Period> &timeout = std::chrono::milliseconds(100),
               const std::chrono::milliseconds &poll_interval = std::chrono::milliseconds(10))
    {
        LINGLONG_TRACE("try lock with timeout");

        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            auto ret = tryLock(type);
            if (!ret) {
                return LINGLONG_ERR(ret);
            }
            if (*ret) {
                return true;
            }
            std::this_thread::sleep_for(poll_interval);
        }
        return false;
    }

    utils::error::Result<void> relock(LockType new_type) noexcept;

    utils::error::Result<void> unlock() noexcept;

    [[nodiscard]] LockType type() const noexcept { return type_; }

    [[nodiscard]] bool isLocked() const noexcept;

    [[nodiscard]] pid_t pid() const noexcept;

private:
    [[nodiscard]] utils::error::Result<void> lockCheck() const noexcept;

    FileLock(int fd, std::filesystem::path path) noexcept;

    pid_t pid_{ ::getpid() };
    LockType type_{ LockType::Read };
    std::atomic_bool locked{ false };
    int fd{ -1 };
    std::filesystem::path path;

    static inline std::pair<pid_t, std::unordered_map<std::string, bool>> process_locked_paths{
        ::getpid(), {}
    };
};

} // namespace linglong::utils::filelock

template <>
struct fmt::formatter<linglong::utils::filelock::LockType>
{
    constexpr auto parse(fmt::format_parse_context &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const linglong::utils::filelock::LockType &p, FormatContext &ctx) const
    {
        const auto *msg = p == linglong::utils::filelock::LockType::Read ? "read" : "write";
        return fmt::format_to(ctx.out(), "{}", msg);
    }
};
