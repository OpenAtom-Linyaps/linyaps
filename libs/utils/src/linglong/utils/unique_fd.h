// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <unistd.h>

namespace linglong::utils::fd {

class UniqueFd
{
public:
    explicit UniqueFd(int fd = -1) noexcept
        : fd_(fd)
    {
    }

    UniqueFd(const UniqueFd &) = delete;
    UniqueFd &operator=(const UniqueFd &) = delete;

    UniqueFd(UniqueFd &&other) noexcept
        : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    UniqueFd &operator=(UniqueFd &&other) noexcept
    {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    ~UniqueFd() noexcept { reset(); }

    void reset(int fd = -1) noexcept
    {
        if (fd_ != -1) {
            ::close(fd_);
        }
        fd_ = fd;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }

    [[nodiscard]] int release() noexcept
    {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return fd_ != -1; }

private:
    int fd_{ -1 };
};

} // namespace linglong::utils::fd
