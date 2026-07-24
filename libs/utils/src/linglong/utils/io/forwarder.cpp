// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/utils/io/forwarder.h"

#include "linglong/utils/io/event_loop.h"
#include "linglong/utils/log/log.h"

namespace linglong::utils::io {

IOForwarder::IOForwarder(EventLoop &epoll, std::size_t bufferSize) noexcept
    : buffer_(RingBuffer::create(bufferSize))
    , epoll_(&epoll)
{
}

IOForwarder::~IOForwarder() noexcept
{
    if (dstFd_ >= 0 && writeWatched_) {
        epoll_->remove(dstFd_);
    }
}

void IOForwarder::setSrc(int fd) noexcept
{
    srcFd_ = fd;
}

void IOForwarder::setDst(int fd) noexcept
{
    dstFd_ = fd;
}

void IOForwarder::pull() noexcept
{
    if (srcEof_ || !buffer_ || buffer_->full() || srcFd_ < 0) {
        return;
    }

    while (!buffer_->full()) {
        auto iov = buffer_->writeIovecs();
        int iovcnt = (iov[1].iov_len > 0) ? 2 : 1;
        auto n = ::readv(srcFd_, iov.data(), iovcnt);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }

            // EIO
            srcEof_ = true;
            return;
        }

        if (n == 0) {
            srcEof_ = true;
            return;
        }

        buffer_->commitWrite(static_cast<std::size_t>(n));
    }
}

void IOForwarder::push() noexcept
{
    if (dstFailed_ || !buffer_ || buffer_->empty() || dstFd_ < 0) {
        return;
    }

    while (!buffer_->empty()) {
        auto iov = buffer_->readIovecs();
        int iovcnt = (iov[1].iov_len > 0) ? 2 : 1;
        auto n = ::writev(dstFd_, iov.data(), iovcnt);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                registerDstWrite();
                return;
            }

            // EIO
            dstFailed_ = true;
            buffer_->clear();
            return;
        }

        if (n == 0) {
            dstFailed_ = true;
            buffer_->clear();
            return;
        }

        buffer_->commitRead(static_cast<std::size_t>(n));
    }

    if (writeWatched_) {
        unregisterDstWrite();
    }
}

void IOForwarder::drive() noexcept
{
    pull();
    push();
}

void IOForwarder::onDstWritable() noexcept
{
    push();
}

void IOForwarder::registerDstWrite() noexcept
{
    if (writeWatched_ || dstFd_ < 0) {
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLOUT;
    ev.data.fd = dstFd_;
    if (::epoll_ctl(epoll_->nativeHandle(), EPOLL_CTL_ADD, dstFd_, &ev) == 0) {
        writeWatched_ = true;
    }
}

void IOForwarder::unregisterDstWrite() noexcept
{
    if (!writeWatched_) {
        return;
    }

    epoll_->remove(dstFd_);
    writeWatched_ = false;
}

} // namespace linglong::utils::io
