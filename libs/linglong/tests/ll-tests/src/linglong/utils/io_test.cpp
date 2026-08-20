/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "linglong/utils/io/event_loop.h"
#include "linglong/utils/io/forwarder.h"
#include "linglong/utils/io/pipe.h"
#include "linglong/utils/io/ring_buffer.h"
#include "linglong/utils/signal/signal_blocker.h"
#include "linglong/utils/terminal/terminal_guard.h"
#include "linglong/utils/unique_fd.h"

#include <array>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

namespace {

using linglong::utils::fd::UniqueFd;
using linglong::utils::io::EventLoop;
using linglong::utils::io::IOForwarder;
using linglong::utils::io::Pipe;
using linglong::utils::io::PipeSet;
using linglong::utils::io::RingBuffer;
using linglong::utils::signal::SignalBlocker;
using linglong::utils::terminal::TerminalGuard;

TEST(UniqueFd, DefaultIsInvalid)
{
    UniqueFd fd;
    EXPECT_FALSE(fd);
    EXPECT_EQ(fd.get(), -1);
}

TEST(UniqueFd, ValidWhenOpened)
{
    int raw = ::open("/dev/null", O_RDONLY);
    ASSERT_GE(raw, 0);
    UniqueFd fd{ raw };
    EXPECT_TRUE(static_cast<bool>(fd));
    EXPECT_EQ(fd.get(), raw);
    EXPECT_TRUE(::fcntl(raw, F_GETFD) != -1);
}

TEST(UniqueFd, MoveTransfersOwnership)
{
    UniqueFd fd{ ::open("/dev/null", O_RDONLY) };
    ASSERT_TRUE(fd);
    int raw = fd.get();

    UniqueFd moved{ std::move(fd) };
    EXPECT_EQ(moved.get(), raw);
    EXPECT_EQ(fd.get(), -1);
}

TEST(UniqueFd, MoveAssignmentClosesOld)
{
    UniqueFd first{ ::open("/dev/null", O_RDONLY) };
    UniqueFd second{ ::open("/dev/null", O_RDONLY) };
    int firstRaw = first.get();

    second = std::move(first);
    EXPECT_EQ(second.get(), firstRaw);
    EXPECT_EQ(first.get(), -1);
}

TEST(UniqueFd, ReleaseTransfersOut)
{
    UniqueFd fd{ ::open("/dev/null", O_RDONLY) };
    int raw = fd.get();
    auto released = fd.release();
    EXPECT_EQ(released, raw);
    EXPECT_EQ(fd.get(), -1);
    ::close(released);
}

TEST(UniqueFd, ResetClosesAndSetsNew)
{
    UniqueFd fd{ ::open("/dev/null", O_RDONLY) };
    ASSERT_TRUE(fd);
    fd.reset(-1);
    EXPECT_FALSE(static_cast<bool>(fd));

    int raw = ::open("/dev/null", O_RDONLY);
    fd.reset(raw);
    EXPECT_EQ(fd.get(), raw);
}

TEST(SignalBlocker, BlocksAndRestores)
{
    SignalBlocker blocker{ SIGINT, SIGTERM };
    EXPECT_TRUE(blocker.valid());

    sigset_t current{};
    ::pthread_sigmask(SIG_SETMASK, nullptr, &current);
    EXPECT_TRUE(sigismember(&current, SIGINT));
    EXPECT_TRUE(sigismember(&current, SIGTERM));

    blocker.restore();
    ::pthread_sigmask(SIG_SETMASK, nullptr, &current);
    EXPECT_FALSE(sigismember(&current, SIGINT));
}

TEST(TerminalGuard, FailsOnNonTerminalDescriptor)
{
    auto fd = ::open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    auto guard = TerminalGuard::create(fd);
    EXPECT_FALSE(guard.has_value());
    ::close(fd);
}

TEST(TerminalGuard, StaticPropagateWindowSizeFailsWithBadFds)
{
    EXPECT_FALSE(TerminalGuard::propagateWindowSize(-1, -1));
}

TEST(Pipe, CreateAndTransferData)
{
    auto pipe = Pipe::create(O_CLOEXEC);
    ASSERT_TRUE(pipe.has_value());

    int readFd = pipe->readEnd();
    int writeFd = pipe->writeEnd();
    ASSERT_GE(readFd, 0);
    ASSERT_GE(writeFd, 0);

    const std::string payload = "hello pipe";
    EXPECT_EQ(::write(writeFd, payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));

    std::array<char, 64> buf{};
    auto n = ::read(readFd, buf.data(), buf.size());
    ASSERT_GT(n, 0);
    EXPECT_EQ(std::string(buf.data(), n), payload);
}

TEST(Pipe, ReleaseEnds)
{
    auto pipe = Pipe::create();
    ASSERT_TRUE(pipe.has_value());

    int readFd = pipe->releaseReadEnd();
    int writeFd = pipe->releaseWriteEnd();
    EXPECT_EQ(pipe->readEnd(), -1);
    EXPECT_EQ(pipe->writeEnd(), -1);
    ::close(readFd);
    ::close(writeFd);
}

TEST(Pipe, CloseEnds)
{
    auto pipe = Pipe::create();
    ASSERT_TRUE(pipe.has_value());

    EXPECT_TRUE(static_cast<bool>(*pipe));
    pipe->closeWriteEnd();
    pipe->closeReadEnd();
    EXPECT_FALSE(static_cast<bool>(*pipe));
}

TEST(PipeSet, CreateProducesThreePipes)
{
    auto set = PipeSet::create();
    ASSERT_TRUE(set.has_value());
    EXPECT_TRUE(set->stdinPipe.has_value());
    EXPECT_TRUE(set->stdoutPipe.has_value());
    EXPECT_TRUE(set->stderrPipe.has_value());
}

TEST(RingBuffer, CreateRejectsZeroCapacity)
{
    EXPECT_FALSE(RingBuffer::create(0).has_value());
}

TEST(RingBuffer, RoundsUpToPowerOfTwo)
{
    auto buf = RingBuffer::create(1);
    ASSERT_TRUE(buf.has_value());
    EXPECT_EQ(buf->capacity(), 1);

    auto big = RingBuffer::create(1000);
    ASSERT_TRUE(big.has_value());
    EXPECT_EQ(big->capacity(), 1024);
}

TEST(RingBuffer, EmptyAndFullStates)
{
    auto buf = RingBuffer::create(4);
    ASSERT_TRUE(buf.has_value());
    EXPECT_TRUE(buf->empty());
    EXPECT_FALSE(buf->full());
    EXPECT_EQ(buf->size(), 0);
    EXPECT_EQ(buf->freeSpace(), buf->capacity());
}

TEST(RingBuffer, WriteAndReadViaIovecs)
{
    auto buf = RingBuffer::create(8);
    ASSERT_TRUE(buf.has_value());

    const std::string data = "abcdefgh";
    auto iov = buf->writeIovecs();
    ASSERT_GE(iov[0].iov_len, data.size());
    std::memcpy(iov[0].iov_base, data.data(), data.size());
    buf->commitWrite(data.size());

    EXPECT_FALSE(buf->empty());
    EXPECT_EQ(buf->size(), data.size());

    auto readIov = buf->readIovecs();
    std::string out;
    out.append(static_cast<const char *>(readIov[0].iov_base), readIov[0].iov_len);
    out.append(static_cast<const char *>(readIov[1].iov_base), readIov[1].iov_len);
    EXPECT_EQ(out, data);

    buf->commitRead(data.size());
    EXPECT_TRUE(buf->empty());
}

TEST(RingBuffer, WrapsAround)
{
    auto buf = RingBuffer::create(4);
    ASSERT_TRUE(buf.has_value());

    auto iov = buf->writeIovecs();
    std::memcpy(iov[0].iov_base, "abc", 3);
    buf->commitWrite(3);

    auto readIov = buf->readIovecs();
    std::string first;
    first.append(static_cast<const char *>(readIov[0].iov_base), readIov[0].iov_len);
    first.append(static_cast<const char *>(readIov[1].iov_base), readIov[1].iov_len);
    EXPECT_EQ(first, "abc");
    buf->commitRead(2);
    EXPECT_EQ(buf->size(), 1);
}

TEST(RingBuffer, ClearResetsState)
{
    auto buf = RingBuffer::create(8);
    ASSERT_TRUE(buf.has_value());

    auto iov = buf->writeIovecs();
    std::memcpy(iov[0].iov_base, "xyz", 3);
    buf->commitWrite(3);
    EXPECT_FALSE(buf->empty());

    buf->clear();
    EXPECT_TRUE(buf->empty());
    EXPECT_EQ(buf->size(), 0);
}

TEST(EventLoop, CreateAndWaitTimeout)
{
    auto loop = EventLoop::create();
    ASSERT_TRUE(loop.has_value());
    EXPECT_TRUE(static_cast<bool>(*loop));

    auto result = loop->wait(0);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->count, 0);
}

TEST(EventLoop, AddAndWaitForReadiness)
{
    auto loop = EventLoop::create();
    ASSERT_TRUE(loop.has_value());

    auto pipe = Pipe::create(O_CLOEXEC);
    ASSERT_TRUE(pipe.has_value());

    auto added = loop->add(pipe->readEnd(), EPOLLIN);
    ASSERT_TRUE(added.has_value());
    EXPECT_TRUE(*added);

    auto first = loop->wait(0);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->count, 0);

    const char byte = 'x';
    ASSERT_EQ(::write(pipe->writeEnd(), &byte, 1), 1);

    auto second = loop->wait(100);
    ASSERT_TRUE(second.has_value());
    EXPECT_GT(second->count, 0);
    EXPECT_EQ(second->events[0].data.fd, pipe->readEnd());

    loop->remove(pipe->readEnd());
}

TEST(EventLoop, AddInvalidFdFails)
{
    auto loop = EventLoop::create();
    ASSERT_TRUE(loop.has_value());

    // A negative fd is rejected with an error.
    auto result = loop->add(-1, EPOLLIN);
    EXPECT_FALSE(result.has_value());

    // A regular file does not support epoll: add() reports EPERM as a
    // value of false instead of an error.
    int regularFd = ::open("/dev/null", O_RDONLY);
    ASSERT_GE(regularFd, 0);
    auto notEpollable = loop->add(regularFd, EPOLLIN);
    ASSERT_TRUE(notEpollable.has_value());
    EXPECT_FALSE(*notEpollable);
    ::close(regularFd);
}

TEST(EventLoop, ModifyUnregisteredFdFails)
{
    auto loop = EventLoop::create();
    ASSERT_TRUE(loop.has_value());
    auto result = loop->modify(100000000, EPOLLIN);
    EXPECT_FALSE(result.has_value());
}

TEST(IOForwarder, ForwardsDataFromPipeToPipe)
{
    auto loop = EventLoop::create();
    ASSERT_TRUE(loop.has_value());

    auto src = Pipe::create(O_CLOEXEC | O_NONBLOCK);
    auto dst = Pipe::create(O_CLOEXEC | O_NONBLOCK);
    ASSERT_TRUE(src.has_value());
    ASSERT_TRUE(dst.has_value());

    IOForwarder forwarder{ *loop };
    forwarder.setSrc(src->readEnd());
    forwarder.setDst(dst->writeEnd());
    EXPECT_FALSE(forwarder.isFinished());
    EXPECT_TRUE(forwarder.bufferEmpty());

    const std::string payload = "forward me";
    ASSERT_EQ(::write(src->writeEnd(), payload.data(), payload.size()),
              static_cast<ssize_t>(payload.size()));

    // A single drive() performs both the pull (src -> buffer) and push
    // (buffer -> dst), so after it the buffer must be drained.
    forwarder.drive();
    EXPECT_TRUE(forwarder.bufferEmpty());

    std::array<char, 64> buf{};
    auto n = ::read(dst->readEnd(), buf.data(), buf.size());
    EXPECT_EQ(std::string(buf.data(), n), payload);

    forwarder.markSrcEof();
    EXPECT_TRUE(forwarder.isFinished());

    forwarder.markDstFailed();
    EXPECT_TRUE(forwarder.isFinished());
}

} // namespace
