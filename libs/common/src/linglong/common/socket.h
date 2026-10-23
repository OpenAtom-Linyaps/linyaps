// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <tl/expected.hpp>

#include <string>

namespace linglong::common::socket {

struct SocketData
{
    bool more_data{ false };
    int fd;
    std::string payload;
};

// The caller owns the received descriptor, which is marked close-on-exec.
tl::expected<SocketData, std::string> recvFdWithPayload(int socketFd, std::size_t bufSize = 4096);

tl::expected<void, std::string> sendFdWithPayload(int socketFd, int fd, const std::string &payload);

tl::expected<int, std::string> createUnixSocket(std::string_view path);

} // namespace linglong::common::socket
