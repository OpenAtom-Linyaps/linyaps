// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/common/display.h"

#include "linglong/common/strings.h"
#include "linglong/common/xdg.h"

namespace linglong::common::display {

tl::expected<std::filesystem::path, std::string>
getWaylandDisplay(std::string_view display) noexcept
{
    auto xdgRuntimeDir = xdg::getXDGRuntimeDir();
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

utils::error::Result<XOrgDisplayConf> getXOrgDisplay(std::string_view display) noexcept
{
    LINGLONG_TRACE(fmt::format("parse X DISPLAY {}", display));

    XOrgDisplayConf result{};
    result.screenNo = 0;

    // normalizing display path
    constexpr std::string_view unixPrefix = "unix:";
    if (common::strings::starts_with(display, unixPrefix)) {
        display = display.substr(unixPrefix.size());
    }

    // host[.screen]
    if (!display.empty() && display[0] == '/') {
        result.protocol = "unix";
        std::error_code ec;
        if (std::filesystem::exists(display, ec)) {
            result.host = display;
            return result;
        }

        auto dot = display.find_last_of('.');
        if (dot == std::string_view::npos) {
            return LINGLONG_ERR(fmt::format("{} doesn't exist", display));
        }

        auto sub = display.substr(0, dot);
        if (std::filesystem::exists(sub, ec)) {
            result.host = sub;
            try {
                result.screenNo = std::stoi(std::string(display.substr(dot + 1)));
                if (result.screenNo < 0) {
                    return LINGLONG_ERR(fmt::format("invalid screen No: {}", result.screenNo));
                }
            } catch (const std::exception &e) {
                return LINGLONG_ERR(fmt::format("failed to get screen No: {}", e.what()));
            }
            return result;
        }

        return LINGLONG_ERR(fmt::format("{} doesn't exist", sub));
    }

    // [protocol/][host]:display[.screen]
    auto slash = display.find_last_of('/');
    if (slash != std::string_view::npos) {
        result.protocol = display.substr(0, slash);
        display = display.substr(slash + 1);
    }

    auto colon = display.find_last_of(':');
    if (colon == std::string_view::npos) {
        return LINGLONG_ERR("invalid display");
    }

    auto host = display.substr(0, colon);
    if (!host.empty()) {
        result.host = host;
    }

    display = display.substr(colon + 1);
    try {
        size_t s;
        result.displayNo = std::stoi(std::string(display), &s);
        if (result.displayNo < 0) {
            return LINGLONG_ERR(fmt::format("invalid display No: {}", result.displayNo));
        }
        display = display.substr(s);
        if (display.size() > 0 && display[0] == '.') {
            result.screenNo = std::stoi(std::string(display.substr(1)));
            if (result.screenNo < 0) {
                return LINGLONG_ERR(fmt::format("invalid screen No: {}", result.screenNo));
            }
        }
    } catch (const std::exception &e) {
        return LINGLONG_ERR(fmt::format("failed to get display No. or screen No: {}", e.what()));
    }

    return result;
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

} // namespace linglong::common::display
