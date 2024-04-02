// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "nlohmann/json.hpp"

#include <iostream>

int main()
{
    nlohmann::json content;
    std::string ociVersion;
    try {
        content = nlohmann::json::parse(std::cin);
        ociVersion = content.at("ociVersion");
    } catch (std::exception &exp) {
        std::cerr << exp.what();
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
    std::map<std::string, std::string> roMountMap{
        { "/etc/resolv.conf", "/run/host/network/etc/resolv.conf" },
        { "/etc/resolvconf", "/run/resolvconf" },
        { "/usr/lib/locale/", "/usr/lib/locale/" },
        { "/usr/share/themes", "/usr/share/themes" },
        { "/usr/share/icons", "/usr/share/icons" },
        { "/usr/share/zoneinfo", "/usr/share/zoneinfo" },
        { "/etc/localtime", "/run/host/etc/localtime" },
        { "/etc/machine-id", "/run/host/etc/machine-id" },
        { "/etc/machine-id", "/etc/machine-id" },
        { "/etc/ssl/certs", "/etc/ssl/certs" },
        { "/usr/share/fonts", "/usr/share/fonts" },
        { "/var/cache/fontconfig", "/run/host/appearance/fonts-cache" },
    };

    for (const auto &[source, destination] : roMountMap) {
        mounts.push_back({
          { "type", "bind" },
          { "options", nlohmann::json::array({ "ro", "rbind" }) },
          { "destination", destination },
          { "source", source },
        });
    };

    std::cout << content.dump() << std::endl;
    return 0;
}
