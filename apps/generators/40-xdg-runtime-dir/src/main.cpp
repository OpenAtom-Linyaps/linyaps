/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

#include <iostream>
#include <optional>

#include <unistd.h>

int main()
{
    auto uid = ::getuid();
    if (uid == 0) {
        std::cerr << "ignore root user";
        return -1;
    }

    using namespace nlohmann::literals;
    nlohmann::json content;
    std::string ociVersion;
    try {
        content = nlohmann::json::parse(std::cin);
        ociVersion = content.at("ociVersion");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << "\n";
        return -1;
    } catch (...) {
        std::cerr << "unknown error occurred during parsing json.";
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched.";
        return -1;
    }

    auto *XDGRuntimeDirEnv = getenv("XDG_RUNTIME_DIR");
    if (XDGRuntimeDirEnv == nullptr) {
        std::cerr << "couldn't get XAUTHORITY from env.";
        return -1;
    }

    auto XDGRuntimeDir = std::string{ "/run/user/" } + std::to_string(uid);

    if (std::strcmp(XDGRuntimeDir.c_str(), XDGRuntimeDirEnv) != 0) {
        std::cerr << "Non default XDG_RUNTIME_DIR is not supported now.";
        return -1;
    }

    nlohmann::json xdgPatch;
    xdgPatch["mounts"] = nlohmann::json::array();

    // tmpfs
    auto &tmpfsMount = xdgPatch["mounts"][0];
    tmpfsMount["destination"] = XDGRuntimeDirEnv;
    tmpfsMount["source"] = "tmpfs";
    tmpfsMount["type"] = "tmpfs";
    tmpfsMount["options"] = nlohmann::json::array({ "nodev", "nosuid", "mode=700" });

    // pulse
    auto &pulseMount = xdgPatch["mounts"][1];
    pulseMount["destination"] = std::string{ XDGRuntimeDirEnv } + "/pulse";
    pulseMount["source"] = std::string{ XDGRuntimeDirEnv } + "/pulse";
    pulseMount["type"] = "bind";
    pulseMount["options"] = nlohmann::json::array({ "rbind" });

    // gvfs
    auto &gvfsMount = xdgPatch["mounts"][2];
    gvfsMount["destination"] = std::string{ XDGRuntimeDirEnv } + "/gvfs";
    gvfsMount["source"] = std::string{ XDGRuntimeDirEnv } + "/gvfs";
    gvfsMount["type"] = "bind";
    gvfsMount["options"] = nlohmann::json::array({ "rbind" });

    // wayland display
    auto waylandDisplay = [XDGRuntimeDirEnv]() -> std::optional<nlohmann::json> {
        auto *waylandDisplayEnv = getenv("WAYLAND_DISPLAY");
        if (waylandDisplayEnv == nullptr) {
            std::cerr << "couldn't get WAYLAND_DISPLAY from Env";
            return std::nullopt;
        }

        auto socketPath =
          std::filesystem::path(std::string{ XDGRuntimeDirEnv } + "/" + waylandDisplayEnv);
        if (!std::filesystem::exists(socketPath)) {
            std::cerr << "Wayland display socket not found at " << socketPath;
            return std::nullopt;
        }

        auto ret = u8R"(
                {
                    "type": "bind",
                    "options": [ "rbind" ]
                }
        )"_json;
        ret["destination"] = socketPath.string();
        ret["source"] = socketPath.string();

        return ret;
    }();

    if (waylandDisplay) {
        xdgPatch["mounts"].emplace_back(std::move(waylandDisplay).value());
    }

    // session bus
    auto sessionBusSocket = [XDGRuntimeDirEnv]() -> std::optional<nlohmann::json> {
        auto *sessionBusEnv = getenv("DBUS_SESSION_BUS_ADDRESS");
        if (sessionBusEnv == nullptr) {
            std::cerr << "couldn't get WAYLAND_DISPLAY from Env";
            return std::nullopt;
        }

        auto sessionBus = std::string_view{ sessionBusEnv };
        auto suffix = std::string_view{ "unix:path=" };
        if (sessionBus.rfind(suffix, 0) != 0) {
            std::cerr << "Unexpected DBUS_SESSION_BUS_ADDRESS=" << sessionBus;
            return std::nullopt;
        }

        auto socketPath = std::filesystem::path(sessionBus.substr(suffix.size()));
        if (!std::filesystem::exists(socketPath)) {
            std::cerr << "D-Bus session bus socket not found at " << socketPath;
            return std::nullopt;
        }

        if (socketPath.string().rfind(XDGRuntimeDirEnv) != 0U) {
            std::cerr << "D-Bus session bus socket not in XDG_RUNTIME_DIR is not supported.";
            return std::nullopt;
        }

        auto ret = u8R"(
                {
                    "type": "bind",
                    "options": [ "rbind" ]
                }
        )"_json;
        ret["destination"] = socketPath.string();
        ret["source"] = socketPath.string();

        return ret;
    }();

    if (sessionBusSocket) {
        xdgPatch["mounts"].emplace_back(std::move(sessionBusSocket).value());
    }

    // dconf
    auto dconf = [XDGRuntimeDirEnv]() -> std::optional<nlohmann::json> {
        auto dconfPath = std::string{ XDGRuntimeDirEnv } + "/dconf";
        if (!std::filesystem::exists(dconfPath)) {
            std::cerr << "dconf directory not found at " << dconfPath;
            return std::nullopt;
        }

        auto ret = u8R"(
                {
                    "type": "bind",
                    "options": [ "rbind" ]
                }
        )"_json;
        ret["destination"] = dconfPath;
        ret["source"] = dconfPath;

        return ret;
    }();

    if (dconf) {
        xdgPatch["mounts"].emplace_back(std::move(dconf).value());
    }

    content.merge_patch(xdgPatch);
    std::cout << std::setw(4) << content;
    return 0;
}
