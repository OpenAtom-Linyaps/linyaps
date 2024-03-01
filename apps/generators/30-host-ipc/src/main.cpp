/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "nlohmann/json.hpp"

#include <iostream>
#include <optional>

int main()
{
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

    auto ipcPatch = u8R"(
    {
        "mounts": [ {
            "destination": "/tmp/.X11-unix",
            "type": "bind",
            "source": "/tmp/.X11-unix",
            "options": [
                    "rbind"
            ]
        } ]
    })"_json;

    auto mountPatch = u8R"(
                {
                    "type": "bind",
                    "options": [
                            "rbind"
                    ]
                }
        )"_json;

    auto dbusMount = [dbusPatch = mountPatch]() mutable -> std::optional<nlohmann::json> {
        const auto *systemBus = u8"/run/dbus/system_bus_socket";
        auto *systemBusEnv = getenv("DBUS_SYSTEM_BUS_ADDRESS");
        if (systemBusEnv == nullptr) {
            std::cerr << "couldn't get DBUS_SYSTEM_BUS_ADDRESS from env";
            return std::nullopt;
        }

        if (std::strcmp(systemBus, systemBusEnv) != 0) {
            std::cerr << "Non default DBUS_SYSTEM_BUS_ADDRESS $DBUS_SYSTEM_BUS_ADDRESS is not "
                         "supported now.";
            return std::nullopt;
        }

        if (!std::filesystem::exists(systemBusEnv)) {
            std::cerr << "D-Bus system bus socket not found at $DBUS_SYSTEM_BUS_ADDRESS.";
            return std::nullopt;
        }

        dbusPatch["destination"] = systemBusEnv;
        dbusPatch["source"] = systemBusEnv;
        return dbusPatch;
    }();

    if (dbusMount) {
        ipcPatch["mounts"].emplace_back(std::move(dbusMount).value());
    }

    auto xauthMount = [xauthPatch = mountPatch]() mutable -> std::optional<nlohmann::json> {
        auto *xauthFileEnv = getenv("XAUTHORITY");
        if (xauthFileEnv == nullptr) {
            std::cerr << "couldn't get XAUTHORITY from env.";
            return std::nullopt;
        }

        auto *homeEnv = getenv("HOME");
        if (homeEnv == nullptr) {
            std::cerr << "couldn't get HOME from env.";
            return std::nullopt;
        }

        auto xauthFile = std::string{ homeEnv } + "/.Xauthority";
        if (std::strcmp(xauthFile.c_str(), xauthFileEnv) != 0) {
            std::cerr << "Non default XAUTHORITY $XAUTHORITY is not supported now.";
            return std::nullopt;
        }

        if (!std::filesystem::exists(xauthFileEnv)) {
            std::cerr << "XAUTHORITY file not found at $XAUTHORITY.";
            return std::nullopt;
        }

        xauthPatch["destination"] = xauthFileEnv;
        xauthPatch["source"] = xauthFileEnv;
        return xauthPatch;
    }();

    if (xauthMount) {
        ipcPatch["mounts"].emplace_back(std::move(xauthMount).value());
    }

    content.merge_patch(ipcPatch);
    std::cout << std::setw(4) << content;
    return 0;
}
