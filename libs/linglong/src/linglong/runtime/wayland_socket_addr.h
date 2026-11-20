// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <cstddef>
#include <string_view>

#include <sys/socket.h>
#include <sys/un.h>

namespace linglong::runtime {

// Build a unix domain socket address for a pathname socket.
//
// sun_path is a fixed size array, so a path that does not fit has to be
// rejected before it is copied: writing past the end of sun_path would
// corrupt whatever lives next to the caller's stack allocated sockaddr_un.
// The guard mirrors the one already present in
// common::socket::createUnixSocket().
[[nodiscard]] linglong::utils::error::Result<struct sockaddr_un>
makeWaylandSocketAddr(std::string_view path) noexcept;

// Length of the address returned by makeWaylandSocketAddr(), including the
// terminating NUL byte, to be passed to bind(2) or connect(2).
//
// Note that this is not necessarily equal to sizeof(struct sockaddr_un): only
// the bytes that actually name the socket are handed to the kernel.
[[nodiscard]] constexpr socklen_t waylandSocketAddrLength(std::string_view path) noexcept
{
    return static_cast<socklen_t>(offsetof(struct sockaddr_un, sun_path) + path.size() + 1);
}

} // namespace linglong::runtime
