// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/runtime/wayland_socket_addr.h"

#include <fmt/format.h>

#include <algorithm>

namespace linglong::runtime {

linglong::utils::error::Result<struct sockaddr_un>
makeWaylandSocketAddr(std::string_view path) noexcept
{
    LINGLONG_TRACE("make wayland socket address");

    if (path.empty()) {
        return LINGLONG_ERR("wayland socket path is empty");
    }

    struct sockaddr_un addr{};
    constexpr auto capacity = sizeof(addr.sun_path);

    // A path of `capacity` bytes or more leaves no room for the terminating
    // NUL byte inside sun_path, so it cannot be represented at all.
    if (path.size() >= capacity) {
        return LINGLONG_ERR(fmt::format("wayland socket path is too long: {}", path));
    }

    addr.sun_family = AF_UNIX;
    std::copy(path.cbegin(), path.cend(), addr.sun_path);
    addr.sun_path[path.size()] = '\0';

    return addr;
}

} // namespace linglong::runtime
