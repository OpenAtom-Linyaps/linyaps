// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "25_host_rootfs.h"

#include "ocppi/runtime/config/types/Generators.hpp"

#include <iostream>

namespace linglong::generator {

bool HostRootfs::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    auto rawPatch = R"({
    "ociVersion": "1.0.1",
    "patch": [
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/run/host",
                "type": "tmpfs",
                "source": "tmpfs",
                "options": ["nodev", "nosuid", "mode=700"]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/run/host/rootfs",
                "type": "bind",
                "source": "/",
                "options": ["rbind"]
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
