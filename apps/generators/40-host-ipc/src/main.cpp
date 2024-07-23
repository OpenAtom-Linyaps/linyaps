/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>
#include <string>

#include <pwd.h>
#include <sys/stat.h>
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
    auto bindIfExist = [&mounts](std::string_view source, std::string_view destination) mutable {
        if (!std::filesystem::exists(source)) {
            return;
        }

        auto realDest = destination.empty() ? source : destination;
        mounts.push_back({ { "source", source },
                           { "type", "bind" },
                           { "destination", realDest },
                           { "options", nlohmann::json::array({ "rbind" }) } });
    };

    bindIfExist("/tmp/.X11-unix", "");

    auto mount = R"({
        "type": "bind",
        "options": [
            "rbind"
        ]
    })"_json;

    [dbusMount = mount, &content]() mutable {
        auto *systemBusEnv = getenv("DBUS_SYSTEM_BUS_ADDRESS"); // NOLINT

        // https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-types:~:text=the%20default%20locations.-,System%20message%20bus,-A%20computer%20may
        std::string systemBus{ "/var/run/dbus/system_bus_socket" };
        if (systemBusEnv != nullptr && std::filesystem::exists(systemBusEnv)) {
            systemBus = systemBusEnv;
        }

        if (!std::filesystem::exists(systemBus)) {
            std::cerr << "D-Bus system bus socket not found at " << systemBus << std::endl;
            return;
        }

        dbusMount["destination"] = "/run/dbus/system_bus_socket";
        dbusMount["source"] = systemBus;
        content["mounts"].emplace_back(std::move(dbusMount));
        content["process"]["env"].emplace_back(
          "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket");
    }();

    mounts.push_back({
      { "destination", "/run/user" },
      { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
      { "source", "tmpfs" },
      { "type", "tmpfs" },
    });

    bool xdgRuntimeDirMounted = false;

    [mount, &mounts, &xdgRuntimeDirMounted, &content, &bindIfExist]() {
        auto *XDGRuntimeDirEnv = getenv("XDG_RUNTIME_DIR"); // NOLINT
        if (XDGRuntimeDirEnv == nullptr) {
            return;
        }

        auto hostXDGRuntimeDir = std::filesystem::path{ XDGRuntimeDirEnv };
        auto status = std::filesystem::status(hostXDGRuntimeDir);
        using perm = std::filesystem::perms;
        if (status.permissions() != perm::owner_all) {
            std::cerr << "The Unix permission of " << hostXDGRuntimeDir << "must be 0700."
                      << std::endl;
            return;
        }

        struct stat64 buf
        {
        };
        if (::stat64(hostXDGRuntimeDir.string().c_str(), &buf) != 0) {
            std::cerr << "Failed to get state of " << hostXDGRuntimeDir << ": " << ::strerror(errno)
                      << std::endl;
            return;
        }

        if (buf.st_uid != ::getuid()) {
            std::cerr << hostXDGRuntimeDir << " doesn't belong to current user.";
            return;
        }

        auto cognitiveXDGRuntimeDir =
          std::filesystem::path{ "/run/user" } / std::to_string(::getuid());

        // tmpfs
        mounts.push_back(nlohmann::json::object({
          { "destination", cognitiveXDGRuntimeDir },
          { "source", "tmpfs" },
          { "type", "tmpfs" },
          { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=700" }) },
        }));

        content["process"]["env"].emplace_back(std::string{ "XDG_RUNTIME_DIR=" }
                                               + cognitiveXDGRuntimeDir.string());

        xdgRuntimeDirMounted = true;

        bindIfExist((hostXDGRuntimeDir / "pulse").string(),
                    (cognitiveXDGRuntimeDir / "pulse").string());
        bindIfExist((hostXDGRuntimeDir / "gvfs").string(),
                    (cognitiveXDGRuntimeDir / "gvfs").string());

        [&hostXDGRuntimeDir, &cognitiveXDGRuntimeDir, &mounts]() {
            auto *waylandDisplayEnv = getenv("WAYLAND_DISPLAY"); // NOLINT
            if (waylandDisplayEnv == nullptr) {
                std::cerr << "Couldn't get WAYLAND_DISPLAY." << std::endl;
                return;
            }

            auto socketPath = std::filesystem::path(hostXDGRuntimeDir) / waylandDisplayEnv;
            if (!std::filesystem::exists(socketPath)) {
                std::cerr << "Wayland display socket not found at " << socketPath << "."
                          << std::endl;
                return;
            }
            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", cognitiveXDGRuntimeDir / waylandDisplayEnv },
              { "source", socketPath.string() },
            }));
        }();

        [&cognitiveXDGRuntimeDir, &mounts, &content]() {
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

            auto hostSessionBus = socketPath.string();
            auto cognitiveSessionBus = cognitiveXDGRuntimeDir / "bus";
            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", cognitiveSessionBus },
              { "source", hostSessionBus },
            }));

            content["process"]["env"].emplace_back(std::string{ "DBUS_SESSION_BUS_ADDRESS=" }
                                                   + "unix:path=" + cognitiveSessionBus.string());
        }();

        [&hostXDGRuntimeDir, &cognitiveXDGRuntimeDir, &mounts]() {
            auto dconfPath = std::filesystem::path(hostXDGRuntimeDir) / "dconf";
            if (!std::filesystem::exists(dconfPath)) {
                std::cerr << "dconf directory not found at " << dconfPath << "." << std::endl;
                return;
            }
            mounts.emplace_back(nlohmann::json::object({
              { "type", "bind" },
              { "options", nlohmann::json::array({ "rbind" }) },
              { "destination", cognitiveXDGRuntimeDir / "dconf" },
              { "source", dconfPath.string() },
            }));
        }();
    }();

    [xauthPatch = mount, &mounts, xdgRuntimeDirMounted, &content]() mutable {
        auto *homeEnv = ::getenv("HOME"); // NOLINT
        if (homeEnv == nullptr) {
            std::cerr << "Couldn't get HOME from env." << std::endl;
            return;
        }

        auto *userEnv = ::getenv("USER");
        if (userEnv == nullptr) {
            std::cerr << "Couldn't get USER from env." << std::endl;
            return;
        }

        auto hostXauthFile = std::string{ homeEnv } + "/.Xauthority";
        auto cognitiveXauthFile = std::string{ "/home/" } + userEnv + "/.Xauthority";

        auto *xauthFileEnv = getenv("XAUTHORITY"); // NOLINT
        if (xauthFileEnv != nullptr && std::filesystem::exists(xauthFileEnv)) {
            hostXauthFile = xauthFileEnv;
        }

        if (hostXauthFile.rfind(homeEnv, 0) != 0U
            && ((!xdgRuntimeDirMounted)
                || hostXauthFile.rfind("/run/user/" + std::to_string(::getuid()), 0) != 0U)) {
            std::cerr << "XAUTHORITY equals to " << hostXauthFile << " is not supported now."
                      << std::endl;
            return;
        }

        if (!std::filesystem::exists(hostXauthFile)) {
            std::cerr << "XAUTHORITY file not found at " << hostXauthFile << "." << std::endl;
            return;
        }

        xauthPatch["destination"] = cognitiveXauthFile;
        xauthPatch["source"] = hostXauthFile;

        mounts.emplace_back(std::move(xauthPatch));
        content["process"]["env"].emplace_back("XAUTHORITY=" + cognitiveXauthFile);
        return;
    }();

    auto pwd = std::filesystem::current_path();
    // [name, destination, target]
    std::vector<std::array<std::string_view, 3>> vec = {
        { "ld.so.cache", "/etc/ld.so.cache", "/run/linglong/etc/ld.so.cache" },
        { "localtime", "/etc/localtime", "/run/host/rootfs/etc/localtime" },
        { "resolv.conf", "/etc/resolv.conf", "/run/host/rootfs/etc/resolv.conf" },
        { "timezone", "/etc/timezone", "/run/host/rootfs/etc/timezone" },
    };
    for (const auto &[name, destination, target] : vec) {
        auto linkfile = (pwd / name);
        std::error_code ec;
        std::filesystem::create_symlink(target, linkfile.c_str(), ec);
        if (ec) {
            std::cerr << "Failed to create symlink from " << target << " to " << linkfile << ": "
                      << ec.message() << std::endl;
            continue;
        };
        mounts.push_back({
          { "destination", destination },
          { "options", nlohmann::json::array({ "rbind", "ro", "nosymfollow" }) },
          { "source", linkfile.string() },
          { "type", "bind" },
        });
    }
    std::cout << content.dump() << std::endl;
    return 0;
}
