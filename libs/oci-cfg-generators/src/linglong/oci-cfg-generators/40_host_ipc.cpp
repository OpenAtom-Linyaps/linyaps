// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "40_host_ipc.h"

#include <linux/limits.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

    auto onlyApp = config.annotations->find("org.deepin.linglong.onlyApp");
    if (onlyApp != config.annotations->end() && onlyApp->second == "true") {
        return true;
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

    // TODO 应该参考规范文档实现更完善的地址解析支持
    // https://dbus.freedesktop.org/doc/dbus-specification.html#addresses
    [dbusMount = mountTemplate, &mounts, &env]() mutable {
        // default value from
        // https://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-types-system
        std::string systemBusEnv = "unix:path=/var/run/dbus/system_bus_socket";
        if (auto cStr = std::getenv("DBUS_SYSTEM_BUS_ADDRESS"); cStr != nullptr) {
            systemBusEnv = cStr;
        }
        // address 可能是 unix:path=/xxxx,giud=xxx 这种格式
        // 所以先将options部分提取出来，挂载时不需要关心
        std::string options;
        auto optionsPos = systemBusEnv.find(",");
        if (optionsPos != std::string::npos) {
            options = systemBusEnv.substr(optionsPos);
            systemBusEnv.resize(optionsPos);
        }
        auto systemBus = std::string_view{ systemBusEnv };
        auto suffix = std::string_view{ "unix:path=" };
        if (systemBus.rfind(suffix, 0) != 0U) {
            std::cerr << "Unexpected DBUS_SYSTEM_BUS_ADDRESS=" << systemBus << std::endl;
            return;
        }

        auto socketPath = std::filesystem::path(systemBus.substr(suffix.size()));
        if (!std::filesystem::exists(socketPath)) {
            std::cerr << "D-Bus session bus socket not found at " << socketPath << std::endl;
            return;
        }

        dbusMount.destination = "/run/dbus/system_bus_socket";
        dbusMount.source = std::move(socketPath);
        mounts.emplace_back(std::move(dbusMount));
        // 将提取的options再拼到容器中的环境变量
        env.emplace_back("DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket" + options);
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

        struct stat64 buf{};
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

        // TODO 应该参考规范文档实现更完善的地址解析支持
        // https://dbus.freedesktop.org/doc/dbus-specification.html#addresses
        [&cognitiveXDGRuntimeDir, &mounts, &env]() {
            std::string sessionBusEnv;
            if (auto cStr = std::getenv("DBUS_SESSION_BUS_ADDRESS"); cStr != nullptr) {
                sessionBusEnv = cStr;
            }
            if (sessionBusEnv.empty()) {
                std::cerr << "Couldn't get DBUS_SESSION_BUS_ADDRESS" << std::endl;
                return;
            }
            // address 可能是 unix:path=/xxxx,giud=xxx 这种格式
            // 所以先将options部分提取出来，挂载时不需要关心
            std::string options;
            auto optionsPos = sessionBusEnv.find(",");
            if (optionsPos != std::string::npos) {
                options = sessionBusEnv.substr(optionsPos);
                sessionBusEnv.resize(optionsPos);
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
            // 将提取的options再拼到容器中的环境变量
            env.emplace_back(std::string{ "DBUS_SESSION_BUS_ADDRESS=" }
                             + "unix:path=" + cognitiveSessionBus.string() + options);
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
    std::array<std::filesystem::path, 3> vec{
        "/etc/localtime",
        "/etc/resolv.conf",
        "/etc/timezone",
    };

    std::error_code ec;
    for (const auto &destination : vec) {
        auto target = destination;
        auto name = target.filename();

        if (!std::filesystem::exists(target, ec)) {
            if (ec) {
                std::cerr << "Failed to check existence of " << target << ": " << ec.message()
                          << std::endl;
                return false;
            }

            continue;
        }

        auto status = std::filesystem::symlink_status(target, ec);
        if (ec) {
            std::cerr << "Failed to get status of " << target << ": " << ec.message() << std::endl;
            return false;
        }

        if (status.type() == std::filesystem::file_type::symlink) {
            std::array<char, PATH_MAX + 1> buf{};
            auto *rpath = ::realpath(target.c_str(), buf.data());
            if (rpath == nullptr) {
                std::cerr << "Failed to get realpath of " << target << ": " << ::strerror(errno)
                          << std::endl;
                return false;
            }

            target.assign(rpath);
        }

        auto linkfile = bundleDir / name;
        target = "/run/host/rootfs" / target.lexically_relative("/");
        std::filesystem::create_symlink(target, linkfile, ec);
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

    // process ld.so.cache
    auto ldLink = bundleDir / "ld.so.cache";
    std::filesystem::create_symlink("/run/linglong/cache/ld.so.cache", ldLink, ec);
    if (ec) {
        std::cerr << "Failed to create symlink from " << "/run/linglong/cache/ld.so.cache" << " to "
                  << ldLink << ": " << ec.message() << std::endl;
        return false;
    }

    mounts.push_back(ocppi::runtime::config::types::Mount{
      .destination = "/etc/ld.so.cache",
      .options = string_list{ "rbind", "ro", "nosymfollow", "copy-symlink" },
      .source = ldLink,
      .type = "bind",
    });

    process.env = std::move(env);
    config.process = std::move(process);
    config.mounts = std::move(mounts);

    return true;
}

} // namespace linglong::generator
