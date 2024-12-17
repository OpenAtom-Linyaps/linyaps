// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "40_host_ipc.h"

#include <linux/limits.h>

#include <filesystem>
#include <iostream>

#include <sys/stat.h>
#include <unistd.h>

namespace linglong::generator {

bool HostIPC::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (config.ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return false;
    }

    if (!config.annotations) {
        std::cerr << "no annotations." << std::endl;
        return false;
    }

    auto it = config.annotations->find("org.deepin.linglong.bundleDir");
    if (it == config.annotations->end()) {
        std::cerr << "bundleDir not found." << std::endl;
        return false;
    }

    if (it->second.empty()) {
        std::cerr << "appID is empty." << std::endl;
        return false;
    }

    auto bundleDir = std::filesystem::path{ it->second };
    auto mounts = config.mounts.value_or(std::vector<ocppi::runtime::config::types::Mount>{});
    auto process = config.process.value_or(ocppi::runtime::config::types::Process{});
    auto env = process.env.value_or(std::vector<std::string>{});

    auto bindIfExist = [&mounts](std::string_view source, std::string_view destination) mutable {
        if (!std::filesystem::exists(source)) {
            return;
        }

        auto realDest = destination.empty() ? source : destination;
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = std::string{ realDest },
          .options = string_list{ "rbind" },
          .source = std::string{ source },
          .type = "bind",
        });
    };

    bindIfExist("/tmp/.X11-unix", "");

    auto mountTemplate =
      ocppi::runtime::config::types::Mount{ .options = string_list{ "rbind" }, .type = "bind" };

    [dbusMount = mountTemplate, &mounts, &env]() mutable {
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

        dbusMount.destination = "/run/dbus/system_bus_socket";
        dbusMount.source = std::move(systemBus);
        mounts.emplace_back(std::move(dbusMount));
        env.emplace_back("DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket");
    }();

    mounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/run/user",
      .options = string_list{ "nodev", "nosuid", "mode=700" },
      .source = "tmpfs",
      .type = "tmpfs",
    });

    [XDGMount = mountTemplate, &mounts, &env, &bindIfExist]() {
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
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = cognitiveXDGRuntimeDir,
          .options = string_list{ "nodev", "nosuid", "mode=700" },
          .source = "tmpfs",
          .type = "tmpfs",
        });
        env.emplace_back(std::string{ "XDG_RUNTIME_DIR=" } + cognitiveXDGRuntimeDir.string());

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
            mounts.emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveXDGRuntimeDir / waylandDisplayEnv,
              .options = string_list{ "rbind" },
              .source = socketPath,
              .type = "bind",
            });
        }();

        [&cognitiveXDGRuntimeDir, &mounts, &env]() {
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

            auto cognitiveSessionBus = cognitiveXDGRuntimeDir / "bus";
            mounts.emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveSessionBus,
              .options = string_list{ "rbind" },
              .source = socketPath,
              .type = "bind",
            });

            env.emplace_back(std::string{ "DBUS_SESSION_BUS_ADDRESS=" }
                             + "unix:path=" + cognitiveSessionBus.string());
        }();

        [&hostXDGRuntimeDir, &cognitiveXDGRuntimeDir, &mounts]() {
            auto dconfPath = std::filesystem::path(hostXDGRuntimeDir) / "dconf";
            if (!std::filesystem::exists(dconfPath)) {
                std::cerr << "dconf directory not found at " << dconfPath << "." << std::endl;
                return;
            }
            mounts.emplace_back(ocppi::runtime::config::types::Mount{
              .destination = cognitiveXDGRuntimeDir / "dconf",
              .options = string_list{ "rbind" },
              .source = dconfPath.string(),
              .type = "bind",
            });
        }();
    }

    ();

    [xauthPatch = mountTemplate, &mounts, &env]() mutable {
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

        auto *xauthFileEnv = ::getenv("XAUTHORITY"); // NOLINT
        std::error_code ec;
        if (xauthFileEnv != nullptr && std::filesystem::exists(xauthFileEnv, ec)) {
            hostXauthFile = xauthFileEnv;
        }

        if (!std::filesystem::exists(hostXauthFile, ec) || ec) {
            std::cerr << "XAUTHORITY file not found at " << hostXauthFile << ":" << ec.message()
                      << std::endl;
            return;
        }

        env.emplace_back("XAUTHORITY=" + cognitiveXauthFile);

        xauthPatch.destination = std::move(cognitiveXauthFile);
        xauthPatch.source = hostXauthFile;
        mounts.emplace_back(std::move(xauthPatch));
    }();
    // 在容器中把易变的文件挂载成软链接，指向/run/host/rootfs，实现实时响应

    // 如果/etc/localtime是嵌套软链会导致chromium时区异常，需要特殊处理
    std::string localtimePath = "/run/host/rootfs/etc/localtime";
    if (std::filesystem::is_symlink("/etc/localtime")) {
        std::array<char, PATH_MAX + 1> buf{};
        auto *target = ::realpath("/etc/localtime", buf.data());
        if (target == nullptr) {
            std::cerr << "Failed to get realpath of /etc/localtime: " << ::strerror(errno)
                      << std::endl;
            return -1;
        }

        auto absoluteTarget = std::filesystem::path{ target }.lexically_relative("/");
        localtimePath = "/run/host/rootfs" / absoluteTarget;
    }
    // 为 /run/linglong/etc/ld.so.cache 创建父目录
    mounts.push_back(
      ocppi::runtime::config::types::Mount{ .destination = "/run/linglong/etc",
                                            .options = string_list{ "nodev", "nosuid", "mode=700" },
                                            .source = "tmpfs",
                                            .type = "tmpfs" });

    // [name, destination, target]
    std::vector<std::array<std::string_view, 3>> vec = {
        { "ld.so.cache", "/etc/ld.so.cache", "/run/linglong/cache/ld.so.cache" },
        { "localtime", "/etc/localtime", localtimePath },
        { "resolv.conf", "/etc/resolv.conf", "/run/host/rootfs/etc/resolv.conf" },
        { "timezone", "/etc/timezone", "/run/host/rootfs/etc/timezone" },
    };
    for (const auto &[name, destination, target] : vec) {
        auto linkfile = bundleDir / name;
        std::error_code ec;
        std::filesystem::create_symlink(target, linkfile.c_str(), ec);
        if (ec) {
            std::cerr << "Failed to create symlink from " << target << " to " << linkfile << ": "
                      << ec.message() << std::endl;
            continue;
        };
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = std::string{ destination },
          .options = string_list{ "rbind", "ro", "nosymfollow", "copy-symlink" },
          .source = linkfile,
          .type = "bind",
        });
    }

    process.env = std::move(env);
    config.process = std::move(process);
    config.mounts = std::move(mounts);

    return true;
}

} // namespace linglong::generator
