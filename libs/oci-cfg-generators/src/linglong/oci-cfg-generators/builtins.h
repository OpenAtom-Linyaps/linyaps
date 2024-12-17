// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "00_id_mapping.h"
#include "05_initialize.h"
#include "10_basics.h"
#include "20_devices.h"
#include "25_host_env.h"
#include "25_host_rootfs.h"
#include "25_host_statics.h"
#include "30_user_home.h"
#include "40_host_ipc.h"
#include "90_legacy.h"

namespace linglong::generator {

using generators = std::map<std::string_view, std::unique_ptr<linglong::generator::Generator>>;

constexpr auto initConfig = R"({
    "ociVersion": "1.0.1",
    "hostname": "linglong",
    "annotations": {
        "org.deepin.linglong.appID": ""
    },
    "root": {
        "path": ""
    },
    "linux": {
        "namespaces": [
            {
                "type": "pid"
            },
            {
                "type": "mount"
            },
            {
                "type": "uts"
            },
            {
                "type": "user"
            }
        ]
    },
    "mounts": [],
    "process": {
        "env": ["LINGLONG_LD_SO_CACHE=/run/linglong/cache/ld.so.cache"],
        "cwd": "/",
        "args": ["bash"]
    }
})";

inline const generators &builtin_generators() noexcept
{
    static std::unique_ptr<generators> gens;
    if (gens) {
        return *gens;
    }

    gens = std::make_unique<generators>();

    auto *id = new IDMapping{};
    gens->emplace(id->name(), id);

    auto *init = new Initialize{};
    gens->emplace(init->name(), init);

    auto *basics = new Basics{};
    gens->emplace(basics->name(), basics);

    auto *dev = new Devices{};
    gens->emplace(dev->name(), dev);

    auto *env = new HostEnv{};
    gens->emplace(env->name(), env);

    auto *rootfs = new HostRootfs{};
    gens->emplace(rootfs->name(), rootfs);

    auto *statics = new HostStatics{};
    gens->emplace(statics->name(), statics);

    auto *home = new UserHome{};
    gens->emplace(home->name(), home);

    auto *ipc = new HostIPC{};
    gens->emplace(ipc->name(), ipc);

    auto *legacy = new Legacy{};
    gens->emplace(legacy->name(), legacy);

    return *gens;
}
} // namespace linglong::generator
