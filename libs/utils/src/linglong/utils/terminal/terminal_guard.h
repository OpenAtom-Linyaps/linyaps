// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <sys/ioctl.h>

#include <optional>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace linglong::utils::terminal {

class TerminalGuard
{
public:
    static std::optional<TerminalGuard> create(int fd) noexcept
    {
        struct termios original{ };
        if (::tcgetattr(fd, &original) != 0) {
            return std::nullopt;
        }

        auto raw = original;
        cfmakeraw(&raw);
        if (::tcsetattr(fd, TCSAFLUSH, &raw) < 0) {
            return std::nullopt;
        }

        return TerminalGuard{ fd, original };
    }

    ~TerminalGuard() noexcept
    {
        if (fd_ >= 0) {
            ::tcsetattr(fd_, TCSAFLUSH, &original_);
        }
    }

    TerminalGuard(const TerminalGuard &) = delete;
    TerminalGuard &operator=(const TerminalGuard &) = delete;

    TerminalGuard(TerminalGuard &&other) noexcept
        : fd_(other.fd_)
        , original_(other.original_)
    {
        other.fd_ = -1;
    }

    TerminalGuard &operator=(TerminalGuard &&other) noexcept
    {
        if (this != &other) {
            if (fd_ >= 0) {
                ::tcsetattr(fd_, TCSAFLUSH, &original_);
            }
            fd_ = other.fd_;
            original_ = other.original_;
            other.fd_ = -1;
        }
        return *this;
    }

    bool setWindowSize(int masterFd) const noexcept
    {
        struct winsize ws{ };
        if (::ioctl(fd_, TIOCGWINSZ, &ws) != 0) {
            return false;
        }
        return ::ioctl(masterFd, TIOCSWINSZ, &ws) == 0;
    }

    static bool propagateWindowSize(int srcFd, int dstFd) noexcept
    {
        struct winsize ws{ };
        if (::ioctl(srcFd, TIOCGWINSZ, &ws) != 0) {
            return false;
        }
        return ::ioctl(dstFd, TIOCSWINSZ, &ws) == 0;
    }

private:
    TerminalGuard(int fd, struct termios original) noexcept
        : fd_(fd)
        , original_(original)
    {
    }

    int fd_{ -1 };
    struct termios original_{ };
};

} // namespace linglong::utils::terminal
