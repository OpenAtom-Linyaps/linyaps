/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>
#include <optional>

#include <unistd.h>

int main()
{
    nlohmann::json content;
    std::string ociVersion;
    try {
        content = nlohmann::json::parse(std::cin);
        ociVersion = content.at("ociVersion");
    } catch (std::exception &exp) {
        std::cerr << exp.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown error occurred during parsing json." << std::endl;
        return -1;
    }

    if (ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return -1;
    }

    auto &mounts = content["mounts"];

    mounts.push_back(u8R"(  {
        "destination": "/tmp/.X11-unix",
        "type": "bind",
        "source": "/tmp/.X11-unix",
        "options": [
                "rbind"
        ]
    } )"_json);

    auto mount = u8R"({
        "type": "bind",
        "options": [
            "rbind"
        ]
    })"_json;

    [dbusMount = mount, &content]() mutable {
        const auto *systemBus = u8"/run/dbus/system_bus_socket";
        auto *systemBusEnv = getenv("DBUS_SYSTEM_BUS_ADDRESS"); // NOLINT
        if (systemBusEnv != nullptr && std::strcmp(systemBus, systemBusEnv) != 0) {
            std::cerr << "Non default DBUS_SYSTEM_BUS_ADDRESS $DBUS_SYSTEM_BUS_ADDRESS is not "
                         "supported now."
                      << std::endl;
            return;
        }

        if (!std::filesystem::exists(systemBus)) {
            std::cerr << "D-Bus system bus socket not found at " << systemBus << std::endl;
            return;
        }

        dbusMount["destination"] = systemBus;
        dbusMount["source"] = systemBus;
        content["mounts"].emplace_back(std::move(dbusMount));
    }();

    mounts.push_back({
      { "destination", "/run/user" },
      { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
      { "source", "tmpfs" },
      { "type", "tmpfs" },
    });

    bool xdgRuntimeDirMounted = false;

    [mount, &mounts, &xdgRuntimeDirMounted]() {
        auto *XDGRuntimeDirEnv = getenv("XDG_RUNTIME_DIR"); // NOLINT

        if (XDGRuntimeDirEnv == nullptr) {
            return;
        }

        auto uid = getuid();
        auto XDGRuntimeDir = "/run/user/" + std::to_string(uid);
        if (std::strcmp(XDGRuntimeDir.c_str(), XDGRuntimeDirEnv) != 0) {
            std::cerr << "Non default XDG_RUNTIME_DIR is not supported now." << std::endl;
            return;
        }

        // tmpfs
        mounts.push_back(nlohmann::json::object({
          { "destination", XDGRuntimeDir },
          { "source", "tmpfs" },
          { "type", "tmpfs" },
          { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
        }));

        xdgRuntimeDirMounted = true;

        auto pulseMount = mount;
        pulseMount["destination"] = XDGRuntimeDir + "/pulse";
        pulseMount["source"] = XDGRuntimeDir + "/pulse";
        mounts.push_back(std::move(pulseMount));

        auto gvfsMount = mount;
        gvfsMount["destination"] = XDGRuntimeDir + "/gvfs";
        gvfsMount["source"] = XDGRuntimeDir + "/gvfs";
        mounts.push_back(std::move(gvfsMount));

        [&XDGRuntimeDir, &mounts]() {
            auto *waylandDisplayEnv = getenv("WAYLAND_DISPLAY"); // NOLINT
            if (waylandDisplayEnv == nullptr) {
                std::cerr << "Couldn't get WAYLAND_DISPLAY." << std::endl;
                return;
            }

            auto socketPath = std::filesystem::path(XDGRuntimeDir) / waylandDisplayEnv;
            if (!std::filesystem::exists(socketPath)) {
                std::cerr << "Wayland display socket not found at " << socketPath << "."
                          << std::endl;
                return;
            }
            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", socketPath.string() },
              { "source", socketPath.string() },
            }));
        }();

        [&XDGRuntimeDir, &mounts]() {
            auto *sessionBusEnv = getenv("DBUS_SESSION_BUS_ADDRESS"); // NOLINT
            if (sessionBusEnv == nullptr) {
                std::cerr << "Couldn't get DBUS_SESSION_BUS_ADDRESS" << std::endl;
                return;
            }

            auto sessionBus = std::string_view{ sessionBusEnv };
            auto suffix = std::string_view{ "unix:path=" };
            if (sessionBus.rfind(suffix, 0) != 0U) {
                std::cerr << "Unexpected DBUS_SESSION_BUS_ADDRESS=" << sessionBus << std::endl;
                return;
            }

            auto socketPath = std::filesystem::path(sessionBus.substr(suffix.size()));
            if (!std::filesystem::exists(socketPath)) {
                std::cerr << "D-Bus session bus socket not found at " << socketPath << std::endl;
                return;
            }

            if (socketPath.string().rfind(XDGRuntimeDir) != 0U) {
                std::cerr << "D-Bus session bus socket not in XDG_RUNTIME_DIR is not supported."
                          << std::endl;
                return;
            }

            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", socketPath.string() },
              { "source", socketPath.string() },
            }));
        }();

        [XDGRuntimeDir, &mounts]() {
            auto dconfPath = std::filesystem::path(XDGRuntimeDir) / "dconf";
            if (!std::filesystem::exists(dconfPath)) {
                std::cerr << "dconf directory not found at " << dconfPath << "." << std::endl;
                return;
            }
            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", dconfPath.string() },
              { "source", dconfPath.string() },
            }));
        }();
    }();

    [xauthPatch = mount, &mounts, xdgRuntimeDirMounted]() mutable {
        auto *homeEnv = getenv("HOME"); // NOLINT
        if (homeEnv == nullptr) {
            std::cerr << "Couldn't get HOME from env." << std::endl;
            return;
        }

        auto xauthFile = std::string{ homeEnv } + "/.Xauthority";

        auto *xauthFileEnv = getenv("XAUTHORITY"); // NOLINT
        if (xauthFileEnv != nullptr) {
            xauthFile = xauthFileEnv;
        }

        if (xauthFile.rfind(homeEnv, 0) != 0U
            && ((!xdgRuntimeDirMounted)
                || xauthFile.rfind("/run/user/" + std::to_string(getuid()), 0) != 0U)) {
            std::cerr << "XAUTHORITY equals to " << xauthFile << " is not supported now."
                      << std::endl;
            return;
        }

        if (!std::filesystem::exists(xauthFile)) {
            std::cerr << "XAUTHORITY file not found at " << xauthFile << "." << std::endl;
            return;
        }

        xauthPatch["destination"] = xauthFile;
        xauthPatch["source"] = xauthFile;

        mounts.emplace_back(std::move(xauthPatch));
        return;
    }();

    std::cout << content.dump() << std::endl;
    return 0;
}
