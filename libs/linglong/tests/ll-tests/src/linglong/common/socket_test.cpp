// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "linglong/common/socket.h"

#include <sys/socket.h>

class SocketFdTest : public ::testing::Test
{
protected:
    std::array<int, 2> sv;

    void SetUp() override { ASSERT_EQ(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv.data()), 0); }

    void TearDown() override
    {
        close(sv[0]);
        close(sv[1]);
    }
};

using namespace linglong::common::socket;

TEST_F(SocketFdTest, NormalCrossProcessTransfer)
{
    const std::string payload = R"({"action": "test-fd-transfer"})";
    const std::string secret = "linglong-security-content";

    auto pid = fork();
    ASSERT_NE(pid, -1);

    if (pid == 0) {
        close(sv[1]);
        std::array<int, 2> pipefds{};
        ASSERT_EQ(pipe(pipefds.data()), 0);

        ASSERT_NE(write(pipefds[1], secret.data(), secret.size()), -1);
        auto res = sendFdWithPayload(sv[0], pipefds[0], payload);

        close(pipefds[0]);
        close(pipefds[1]);
        close(sv[0]);
        _exit(res.has_value() ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], payload.size());

    int status{ 0 };
    waitpid(pid, &status, 0);
    EXPECT_EQ(WEXITSTATUS(status), 0);

    ASSERT_TRUE(res.has_value()) << res.error();
    EXPECT_EQ(res->payload, payload);
    ASSERT_GT(res->fd, 0);

    std::string buf(secret.size(), '\0');
    const ssize_t n = read(res->fd, buf.data(), buf.size());
    ASSERT_NE(n, -1);
    EXPECT_EQ(std::string(buf.data(), n), secret);

    close(res->fd);
}

TEST_F(SocketFdTest, InvalidFileDescriptor)
{
    auto res = recvFdWithPayload(-1, 1024);
    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), "Invalid file descriptor");
}

TEST_F(SocketFdTest, EmptyAncillaryData)
{
    if (fork() == 0) {
        close(sv[1]);
        std::string data = "just-text-no-fd";
        ASSERT_NE(write(sv[0], data.data(), data.size()), -1);
        close(sv[0]);
        _exit(0);
    }

    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    EXPECT_FALSE(res.has_value());
    wait(nullptr);
}

TEST_F(SocketFdTest, PayloadBufferTruncation)
{
    const std::string long_payload(200, 'Z');
    if (fork() == 0) {
        close(sv[1]);
        auto ret = sendFdWithPayload(sv[0], STDOUT_FILENO, long_payload);
        EXPECT_TRUE(ret) << ret.error();
        close(sv[0]);
        _exit(0);
    }
    close(sv[0]);

    auto res = recvFdWithPayload(sv[1], 50);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload.size(), 50);
    EXPECT_EQ(res->payload, long_payload.substr(0, 50));

    if (res->fd >= 0) {
        close(res->fd);
    }

    wait(nullptr);
}

TEST_F(SocketFdTest, SignalInterruptionRecovery)
{
    auto handler = signal(SIGUSR1, [](int) { });
    ASSERT_NE(handler, SIG_ERR);

    if (fork() == 0) {
        close(sv[1]);
        usleep(50000);
        kill(getppid(), SIGUSR1);
        usleep(50000);
        auto ret = sendFdWithPayload(sv[0], STDERR_FILENO, "recovered");
        EXPECT_TRUE(ret) << ret.error();
        close(sv[0]);
        _exit(0);
    }

    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload, "recovered");

    if (res->fd >= 0) {
        close(res->fd);
    }

    wait(nullptr);

    ASSERT_NE(signal(SIGUSR1, handler), SIG_ERR);
}

TEST_F(SocketFdTest, PeerClosedBeforeSending)
{
    if (fork() == 0) {
        close(sv[1]);
        close(sv[0]);
        _exit(0);
    }
    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    EXPECT_FALSE(res.has_value());
    wait(nullptr);
}

TEST_F(SocketFdTest, SendInvalidFdHandle)
{
    auto res = sendFdWithPayload(sv[0], 9999, "bad-fd");
    EXPECT_FALSE(res.has_value());
}

TEST_F(SocketFdTest, LargePayloadHandling)
{
    const std::string large_payload(65536, 'B');
    if (fork() == 0) {
        close(sv[1]);
        auto ret = sendFdWithPayload(sv[0], STDIN_FILENO, large_payload);
        close(sv[0]);
        _exit(0);
    }
    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 70000);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload, large_payload);

    if (res->fd >= 0) {
        close(res->fd);
    }

    wait(nullptr);
}
