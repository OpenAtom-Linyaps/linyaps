// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/display.h"

#include "linglong/common/xdg.h"

namespace linglong::common {

tl::expected<std::filesystem::path, std::string>
getWaylandDisplay(std::string_view display) noexcept
{
    auto xdgRuntimeDir = getXDGRuntimeDir();
    std::filesystem::path waylandSocketPath;
    if (display[0] == '/') {
        waylandSocketPath = display;
    } else {
        waylandSocketPath = xdgRuntimeDir / display;
    }

    std::error_code ec;
    if (!std::filesystem::exists(waylandSocketPath, ec)) {
        if (ec) {
            return tl::unexpected{ "Failed to check Wayland socket path "
                                   + waylandSocketPath.string() + ": " + ec.message() };
        }

        return tl::unexpected{ "Wayland socket path not found at " + waylandSocketPath.string() };
    }

    return waylandSocketPath;
}

tl::expected<std::filesystem::path, std::string> getXOrgDisplay(std::string_view display) noexcept
{
    // normalizing display path
    constexpr std::string_view unixPrefix = "unix:";
    if (display.rfind(unixPrefix, 0) == 0) {
        display = display.substr(unixPrefix.size());
    }

    // explicitly local display
    if (display[0] == '/') {
        return std::filesystem::path{ display };
    }

    // implicitly local display
    if (display[0] == ':') {
        // just hardcode the prefix of socket path, libxcb also does this
        // https://gitlab.freedesktop.org/xorg/lib/libxcb/-/blob/e81b999a727d3c8ee9b83adb7c1c822f67378687/src/xcb_util.c#L252
        const auto *start = display.cbegin() + 1;
        const auto *end = start;
        while (end != display.cend() && (std::isdigit(*end) != 0)) {
            ++end;
        };

        // ignore screen number if it presents
        auto socketName = "X" + std::string(start, end);
        auto socketPath = std::filesystem::path{ "/tmp/.X11-unix" } / socketName;

        std::error_code ec;
        if (!std::filesystem::exists(socketPath, ec)) {
            if (ec) {
                return tl::unexpected{ "Failed to check socket path " + socketPath.string() + ": "
                                       + ec.message() };
            }

            return tl::unexpected{ "X11 socket path not found at " + socketPath.string() };
        }

        return socketPath;
    }

    return std::filesystem::path{};
}

tl::expected<std::filesystem::path, std::string> getXOrgAuthFile(std::string_view authFile) noexcept
{
    std::filesystem::path xAuthFile{ authFile };
    if (xAuthFile.empty()) {
        // fallback to home directory
        auto *home = ::getenv("HOME");
        if (home == nullptr) {
            return tl::unexpected{ "HOME is not set" };
        }

        xAuthFile = std::filesystem::path{ home } / ".Xauthority";
    }

    std::error_code ec;
    auto exists = std::filesystem::exists(xAuthFile, ec);
    if (ec) {
        return tl::unexpected{ "Failed to check XAUTHORITY file " + xAuthFile.string() + ": "
                               + ec.message() };
    }

    if (exists) {
        return xAuthFile;
    }

    return tl::unexpected{ "XAUTHORITY file not found at " + xAuthFile.string() };
}

} // namespace linglong::common
