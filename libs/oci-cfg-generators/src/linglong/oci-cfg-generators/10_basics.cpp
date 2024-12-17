// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "10_basics.h"

#include "ocppi/runtime/config/types/Generators.hpp"

#include <iostream>

namespace linglong::generator {

bool Basics::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    auto rawPatch = R"({
    "ociVersion": "1.0.1",
    "patch": [
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/sys",
                "type": "bind",
                "source": "/sys",
                "options": [
                    "rbind",
                    "nosuid",
                    "noexec",
                    "nodev"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/proc",
                "type": "proc",
                "source": "proc"
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/dev",
                "type": "tmpfs",
                "source": "tmpfs",
                "options": [
                    "nosuid",
                    "strictatime",
                    "mode=0755",
                    "size=65536k"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/dev/pts",
                "type": "devpts",
                "source": "devpts",
                "options": [
                    "nosuid",
                    "noexec",
                    "newinstance",
                    "ptmxmode=0666",
                    "mode=0620"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/dev/shm",
                "type": "tmpfs",
                "source": "shm",
                "options": [
                    "nosuid",
                    "noexec",
                    "nodev",
                    "mode=1777"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/dev/mqueue",
                "type": "bind",
                "source": "/dev/mqueue",
                "options": [
                    "rbind",
                    "nosuid",
                    "noexec",
                    "nodev"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/sys/fs/cgroup",
                "type": "cgroup",
                "source": "cgroup",
                "options": [
                    "nosuid",
                    "noexec",
                    "nodev",
                    "relatime",
                    "ro"
                ]
            }
        },
        {
          "op": "add",
          "path": "/mounts/-",
          "value": {
            "destination": "/run",
            "type": "tmpfs",
            "source": "tmpfs",
            "options": ["nosuid", "strictatime", "mode=0755", "size=65536k"]
          }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/run/udev",
                "type": "bind",
                "source": "/run/udev",
                "options": [
                    "rbind"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/tmp",
                "type": "bind",
                "source": "/tmp",
                "options": [
                    "rbind"
                ]
            }
        },
        {
            "op": "add",
            "path": "/mounts/-",
            "value": {
                "destination": "/etc/passwd",
                "type": "bind",
                "source": "/etc/passwd",
                "options": [
                    "ro",
                    "rbind"
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
