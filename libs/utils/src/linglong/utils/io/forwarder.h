// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/io/ring_buffer.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

namespace linglong::utils::io {

class EventLoop;

class IOForwarder
{
public:
    explicit IOForwarder(EventLoop &epoll, std::size_t bufferSize = 8 * 1024) noexcept;

    IOForwarder(const IOForwarder &) = delete;
    IOForwarder &operator=(const IOForwarder &) = delete;
    IOForwarder(IOForwarder &&) = delete;
    IOForwarder &operator=(IOForwarder &&) = delete;

    ~IOForwarder() noexcept;

    void setSrc(int fd) noexcept;
    void setDst(int fd) noexcept;

    [[nodiscard]] int srcFd() const noexcept { return srcFd_; }
    [[nodiscard]] int dstFd() const noexcept { return dstFd_; }

    void markSrcEof() noexcept { srcEof_ = true; }
    void markDstFailed() noexcept { dstFailed_ = true; }

    [[nodiscard]] bool isFinished() const noexcept
    {
        return (srcEof_ && (buffer_ ? buffer_->empty() : true)) || dstFailed_;
    }

    [[nodiscard]] bool bufferEmpty() const noexcept
    {
        return buffer_ ? buffer_->empty() : true;
    }

    void pull() noexcept;
    void push() noexcept;
    void drive() noexcept;

    [[nodiscard]] bool needsWriteWatch() const noexcept { return writeWatched_; }
    void onDstWritable() noexcept;

private:
    void registerDstWrite() noexcept;
    void unregisterDstWrite() noexcept;

    std::optional<RingBuffer> buffer_;
    int srcFd_{ -1 };
    int dstFd_{ -1 };
    bool srcEof_{ false };
    bool dstFailed_{ false };
    bool writeWatched_{ false };
    EventLoop *epoll_;
};

} // namespace linglong::utils::io
