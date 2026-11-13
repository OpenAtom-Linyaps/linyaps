// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <gtest/gtest.h>

#include "common/tempdir.h"
#include "linglong/common/socket.h"

#include <filesystem>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

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
    auto child = fork();
    ASSERT_NE(child, -1);
    if (child == 0) {
        close(sv[1]);
        std::string data = "just-text-no-fd";
        ASSERT_NE(write(sv[0], data.data(), data.size()), -1);
        close(sv[0]);
        _exit(EXIT_SUCCESS);
    }

    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    EXPECT_FALSE(res.has_value());
    waitpid(child, nullptr, 0);
}

TEST_F(SocketFdTest, PayloadBufferTruncation)
{
    const std::string long_payload(200, 'Z');
    auto child = fork();
    ASSERT_NE(child, -1);
    if (child == 0) {
        close(sv[1]);
        auto ret = sendFdWithPayload(sv[0], STDOUT_FILENO, long_payload);
        EXPECT_TRUE(ret) << ret.error();
        close(sv[0]);
        _exit(EXIT_SUCCESS);
    }
    close(sv[0]);

    auto res = recvFdWithPayload(sv[1], 50);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload.size(), 50);
    EXPECT_EQ(res->payload, long_payload.substr(0, 50));

    if (res->fd >= 0) {
        close(res->fd);
    }

    waitpid(child, nullptr, 0);
}

TEST_F(SocketFdTest, SignalInterruptionRecovery)
{
    struct sigaction sa{};
    sa.sa_handler = []([[maybe_unused]] int sig) -> void { };
    sigemptyset(&sa.sa_mask);
    struct sigaction oldsa{};
    ASSERT_NE(-1, sigaction(SIGUSR1, &sa, &oldsa));

    auto child = fork();
    ASSERT_NE(child, -1);
    if (child == 0) {
        close(sv[1]);
        usleep(50000);
        kill(getppid(), SIGUSR1);
        usleep(50000);
        auto ret = sendFdWithPayload(sv[0], STDERR_FILENO, "recovered");
        EXPECT_TRUE(ret) << ret.error();
        close(sv[0]);
        _exit(EXIT_SUCCESS);
    }

    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload, "recovered");

    if (res->fd >= 0) {
        close(res->fd);
    }

    waitpid(child, nullptr, 0);

    ASSERT_NE(-1, sigaction(SIGUSR1, &oldsa, nullptr));
}

TEST_F(SocketFdTest, PeerClosedBeforeSending)
{
    auto child = fork();
    ASSERT_NE(child, -1);
    if (child == 0) {
        close(sv[1]);
        close(sv[0]);
        _exit(EXIT_SUCCESS);
    }
    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 1024);
    EXPECT_FALSE(res.has_value());
    waitpid(child, nullptr, 0);
}

TEST_F(SocketFdTest, SendInvalidFdHandle)
{
    auto res = sendFdWithPayload(sv[0], 9999, "bad-fd");
    EXPECT_FALSE(res.has_value());
}

TEST_F(SocketFdTest, LargePayloadHandling)
{
    const std::string large_payload(65536, 'B');
    auto child = fork();
    if (child == 0) {
        close(sv[1]);
        auto ret = sendFdWithPayload(sv[0], STDIN_FILENO, large_payload);
        close(sv[0]);
        _exit(EXIT_SUCCESS);
    }
    close(sv[0]);
    auto res = recvFdWithPayload(sv[1], 70000);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->payload, large_payload);

    if (res->fd >= 0) {
        close(res->fd);
    }

    waitpid(child, nullptr, 0);
}

TEST(SocketCreateTest, RejectsEmptyPath)
{
    auto res = createUnixSocket("");

    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), "Path cannot be empty");
}

TEST(SocketCreateTest, RejectsTooLongFilesystemPath)
{
    const std::string path(200, 'a');

    auto res = createUnixSocket(path);

    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), "Path too long");
}

TEST(SocketCreateTest, RejectsTooLongAbstractPath)
{
    std::string path(200, 'a');
    path.front() = '@';

    auto res = createUnixSocket(path);

    EXPECT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), "Path too long");
}

TEST(SocketCreateTest, CreatesFilesystemSocket)
{
    TempDir dir;
    const auto path = dir.path() / "ll-socket";

    auto res = createUnixSocket(path.string());

    ASSERT_TRUE(res.has_value()) << res.error();
    EXPECT_GE(*res, 0);
    EXPECT_TRUE(std::filesystem::is_socket(path));
    close(*res);
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST(SocketCreateTest, SupportsAbstractNamespace)
{
    const std::string path = "@ll-test-abstract-" + std::to_string(::getpid());

    auto res = createUnixSocket(path);

    ASSERT_TRUE(res.has_value()) << res.error();
    EXPECT_GE(*res, 0);
    close(*res);
}

TEST(SocketCreateTest, ReportsBindFailureForMissingParentDirectory)
{
    TempDir dir;
    const auto path = dir.path() / "missing-parent" / "ll-socket";

    auto res = createUnixSocket(path.string());

    EXPECT_FALSE(res.has_value());
    EXPECT_NE(res.error().find("Bind failed"), std::string::npos);
}
