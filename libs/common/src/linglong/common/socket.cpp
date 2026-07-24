// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/socket.h"

#include "linglong/common/error.h"

#include <sys/ioctl.h>

#include <cstring>
#include <filesystem>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace linglong::common::socket {

tl::expected<SocketData, std::string> recvFdWithPayload(int socketFd, std::size_t bufSize)
{
    if (socketFd < 0) {
        return tl::make_unexpected("Invalid file descriptor");
    }

    std::string buffer(bufSize, '\0');
    struct iovec iov{};
    iov.iov_base = buffer.data();
    iov.iov_len = buffer.size();

    alignas(struct cmsghdr) std::array<char, CMSG_SPACE(sizeof(int))> control_buf{};

    struct msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf.data();
    msg.msg_controllen = sizeof(control_buf);

    ssize_t n;
    while (true) {
        n = recvmsg(socketFd, &msg, 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }

        break;
    }

    if (n < 0) {
        return tl::make_unexpected("recvmsg failed: " + error::errorString(errno));
    }

    if (n == 0) {
        return tl::make_unexpected("Connection closed");
    }

    int received_fd{ -1 };
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);

    if (cmsg != nullptr && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        std::memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    }

    if ((static_cast<std::size_t>(msg.msg_flags) & MSG_CTRUNC) != 0) {
        if (received_fd != -1) {
            ::close(received_fd);
        }

        return tl::make_unexpected("Control data truncated");
    }

    if (received_fd == -1) {
        return tl::make_unexpected("No file descriptor received");
    }

    bool isTruncated{ false };
    if ((static_cast<std::size_t>(msg.msg_flags) & MSG_TRUNC) != 0) {
        isTruncated = true;
    } else if (static_cast<std::size_t>(n) == bufSize) {
        int bytes_available{ 0 };
        if (ioctl(socketFd, FIONREAD, &bytes_available) < 0) {
            return tl::make_unexpected("FIONREAD socket failed: " + error::errorString(errno));
        }

        if (bytes_available > 0) {
            isTruncated = true;
        }
    }

    buffer.resize(n);
    return SocketData{ isTruncated, received_fd, std::move(buffer) };
}

tl::expected<void, std::string> sendFdWithPayload(int socketFd, int fd, const std::string &payload)
{
    if (socketFd < 0) {
        return tl::make_unexpected("Invalid socket file descriptor");
    }

    std::size_t totalSent{ 0 };
    bool fdSent{ false };

    while (totalSent < payload.size() || !fdSent) {
        struct msghdr msg{};
        struct iovec iov{};

        iov.iov_base = const_cast<char *>(payload.data() + totalSent);
        iov.iov_len = payload.size() - totalSent;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        alignas(struct cmsghdr) std::array<std::byte, CMSG_SPACE(sizeof(int))> control_buf{};
        if (!fdSent) {
            msg.msg_control = control_buf.data();
            msg.msg_controllen = control_buf.size();

            struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            std::memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
        }

        const auto n = sendmsg(socketFd, &msg, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return tl::make_unexpected("sendmsg failed: " + error::errorString(errno));
        }

        fdSent = true;
        totalSent += n;

        if (payload.empty()) {
            break;
        }
    }

    return {};
}

tl::expected<int, std::string> createUnixSocket(std::string_view path)
{
    if (path.empty()) {
        return tl::make_unexpected("Path cannot be empty");
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    constexpr auto capacity = sizeof(addr.sun_path);

    const bool is_abstract{ (path[0] == '@') };
    size_t addr_len{ 0 };

    if (is_abstract) {
        if (path.size() > capacity) {
            return tl::make_unexpected("Path too long");
        }

        addr.sun_path[0] = '\0';
        std::copy(path.begin() + 1, path.end(), addr.sun_path + 1);

        addr_len = offsetof(struct sockaddr_un, sun_path) + path.size();
    } else {
        if (path.size() >= capacity) {
            return tl::make_unexpected("Path too long");
        }

        std::filesystem::remove(path);
        std::copy(path.cbegin(), path.cend(), addr.sun_path);
        addr.sun_path[path.size()] = '\0';

        addr_len = sizeof(struct sockaddr_un);
    }

    auto fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return tl::make_unexpected("Socket creation failed: " + error::errorString(errno));
    }

    if (::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), addr_len) < 0) {
        int err = errno;
        ::close(fd);
        return tl::make_unexpected("Bind failed: " + std::string(strerror(err)));
    }

    if (::listen(fd, SOMAXCONN) < 0) {
        int err = errno;
        ::close(fd);
        return tl::make_unexpected("Listen failed: " + std::string(strerror(err)));
    }

    return fd;
}

} // namespace linglong::common::socket
