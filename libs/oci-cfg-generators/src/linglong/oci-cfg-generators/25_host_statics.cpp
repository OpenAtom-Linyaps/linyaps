// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "25_host_statics.h"

#include "ocppi/runtime/config/types/Generators.hpp"

#include <iostream>

namespace linglong::generator {
bool HostStatics::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    auto rawPatch = R"({
    "ociVersion": "1.0.1",
    "patch": [
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/usr/share/fonts",
                "type": "bind",
                "source": "/usr/share/fonts",
                "options": [
                    "rbind",
                    "ro"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/usr/share/icons",
                "type": "bind",
                "source": "/usr/share/icons",
                "options": [
                    "rbind",
                    "ro"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/etc/ssl/certs",
                "type": "bind",
                "source": "/etc/ssl/certs",
                "options": [
                    "rbind",
                    "ro"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/etc/machine-id",
                "type": "bind",
                "source": "/etc/machine-id",
                "options": [
                    "rbind",
                    "ro"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/usr/share/themes",
                "type": "bind",
                "source": "/usr/share/themes",
                "options": [
                    "rbind",
                    "ro"
                ]
            }
        }
    ]
})"_json;

    if (config.ociVersion != rawPatch["ociVersion"].get<std::string>()) {
        std::cerr << "ociVersion mismatched" << std::endl;
        return false;
    }

    try {
        auto rawConfig = nlohmann::json(config);
        rawConfig = rawConfig.patch(rawPatch["patch"]);
        config = rawConfig.get<ocppi::runtime::config::types::Config>();
    } catch (const std::exception &e) {
        std::cerr << "patch basics failed:" << e.what() << std::endl;
        return false;
    }

    return true;
}

} // namespace linglong::generator
