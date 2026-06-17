// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"
#include "linglong/utils/unique_fd.h"

#include <fmt/format.h>
#include <sys/epoll.h>

#include <array>
#include <vector>

#include <unistd.h>

namespace linglong::utils::io {

class IOForwarder;

class EventLoop
{
public:
    struct WaitResult
    {
        const struct epoll_event *events;
        int count;
    };

    static utils::error::Result<EventLoop> create(std::size_t maxEvents = 16) noexcept
    {
        LINGLONG_TRACE("create event loop");
        auto fd = ::epoll_create1(EPOLL_CLOEXEC);
        if (fd < 0) {
            return LINGLONG_ERR(
              fmt::format("failed to create epoll: {}", std::string(::strerror(errno))));
        }
        return EventLoop{ fd::UniqueFd{ fd }, maxEvents };
    }

    utils::error::Result<bool> add(int fd, uint32_t events) noexcept
    {
        LINGLONG_TRACE("add fd to epoll");
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        if (::epoll_ctl(epollFd_.get(), EPOLL_CTL_ADD, fd, &ev) == -1) {
            if (errno == EPERM) {
                return false;
            }
            return LINGLONG_ERR(
              fmt::format("failed to add fd {} to epoll: {}", fd, std::string(::strerror(errno))));
        }
        return true;
    }

    void remove(int fd) noexcept { ::epoll_ctl(epollFd_.get(), EPOLL_CTL_DEL, fd, nullptr); }

    utils::error::Result<void> modify(int fd, uint32_t events) noexcept
    {
        LINGLONG_TRACE("modify fd in epoll");
        struct epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        if (::epoll_ctl(epollFd_.get(), EPOLL_CTL_MOD, fd, &ev) == -1) {
            return LINGLONG_ERR(fmt::format("failed to modify fd {} in epoll: {}",
                                            fd,
                                            std::string(::strerror(errno))));
        }
        return LINGLONG_OK;
    }

    utils::error::Result<WaitResult> wait(int timeout = -1) noexcept
    {
        LINGLONG_TRACE("wait for epoll events");
        auto n =
          ::epoll_wait(epollFd_.get(), events_.data(), static_cast<int>(events_.size()), timeout);
        if (n < 0) {
            if (errno == EINTR) {
                return WaitResult{ events_.data(), 0 };
            }
            return LINGLONG_ERR(
              fmt::format("epoll_wait error: {}", std::string(::strerror(errno))));
        }
        return WaitResult{ events_.data(), n };
    }

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(epollFd_); }

    [[nodiscard]] int nativeHandle() const noexcept { return epollFd_.get(); }

private:
    EventLoop(fd::UniqueFd epollFd, std::size_t maxEvents) noexcept
        : epollFd_(std::move(epollFd))
        , events_(maxEvents)
    {
    }

    fd::UniqueFd epollFd_;
    std::vector<struct epoll_event> events_;
};

} // namespace linglong::utils::io
