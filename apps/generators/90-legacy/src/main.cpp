// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <filesystem>
#include <iostream>

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
    std::multimap<std::string, std::string> roMountMap{
        { "/etc/resolv.conf", "/run/host/etc/resolv.conf" },
        { "/etc/resolvconf", "/run/host/etc/resolvconf" },
        { "/etc/localtime", "/run/host/etc/localtime" },
        { "/etc/machine-id", "/run/host/etc/machine-id" },
        { "/etc/machine-id", "/etc/machine-id" },
        { "/etc/ssl/certs", "/run/host/etc/ssl/certs" },
        { "/etc/ssl/certs", "/etc/ssl/certs" },
        { "/var/cache/fontconfig", "/run/host/appearance/fonts-cache" },
        { "/usr/share/fonts", "/usr/share/fonts" },
        { "/usr/lib/locale/", "/usr/lib/locale/" },
        { "/usr/share/themes", "/usr/share/themes" },
        { "/usr/share/icons", "/usr/share/icons" },
        { "/usr/share/zoneinfo", "/usr/share/zoneinfo" },
        { "/etc/resolv.conf", "/etc/resolv.conf" },
        { "/etc/resolvconf", "/etc/resolvconf" },
        { "/etc/localtime", "/etc/localtime" },
    };

    for (const auto &[source, destination] : roMountMap) {
        if (!std::filesystem::exists(source)) {
            std::cerr << source << " not exists on host." << std::endl;
            continue;
        }

        mounts.push_back({
          { "type", "bind" },
          { "options", nlohmann::json::array({ "ro", "rbind" }) },
          { "destination", destination },
          { "source", source },
        });
    };

    {
        // FIXME: com.360.browser-stable
        // 需要一个所有用户都有可读可写权限的目录(/apps-data/private/com.360.browser-stable)
        nlohmann::json annotations;
        std::string appID;
        try {
            annotations = content.at("annotations");
            appID = annotations.at("org.deepin.linglong.appID");
        } catch (std::exception &exp) {
            std::cerr << exp.what() << std::endl;
            return -1;
        }

        if ("com.360.browser-stable" == appID) {
            auto *home = ::getenv("HOME");
            if (home == nullptr) {
                std::cerr << "Couldn't get HOME." << std::endl;
                return -1;
            }

            auto homeDir = std::filesystem::path(home);
            if (!std::filesystem::exists(homeDir)) {
                std::cerr << "Home " << homeDir << "doesn't exists." << std::endl;
                return -1;
            }

            std::error_code ec;
            std::string app360DataSourcePath = homeDir / ".linglong" / appID / "share" / "appdata";

            auto appDataDir = std::filesystem::path(app360DataSourcePath);
            std::filesystem::create_directories(appDataDir, ec);
            if (ec) {
                std::cerr << "Check appDataDir failed:" << ec.message() << std::endl;
                return -1;
            }

            std::string app360DataPath = "/apps-data";
            std::string app360DataDesPath = app360DataPath + "/private/com.360.browser-stable";

            mounts.push_back({
              { "destination", app360DataPath },
              { "options", nlohmann::json::array({ "nodev", "nosuid", "mode=777" }) },
              { "source", "tmpfs" },
              { "type", "tmpfs" },
            });

            mounts.push_back({
              { "destination", app360DataDesPath },
              { "options", nlohmann::json::array({ "rw", "rbind" }) },
              { "source", app360DataSourcePath },
              { "type", "bind" },
            });
        }
    }

    std::cout << content.dump() << std::endl;
    return 0;
}
