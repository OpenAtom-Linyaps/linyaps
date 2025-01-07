// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "20_devices.h"

#include <filesystem>
#include <iostream>

namespace linglong::generator {

bool Devices::generate(ocppi::runtime::config::types::Config &config) const noexcept
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

    auto mounts = config.mounts.value_or(std::vector<ocppi::runtime::config::types::Mount>{});

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

    bindIfExist("/run/udev", "");
    bindIfExist("/dev/snd", "");
    bindIfExist("/dev/dri", "");

    for (const auto &entry : std::filesystem::directory_iterator{ "/dev" }) {
        const auto &devPath = entry.path();
        auto devName = devPath.filename().string();
        if ((devName.rfind("video", 0) == 0) || (devName.rfind("nvidia", 0) == 0)) {
            mounts.emplace_back(ocppi::runtime::config::types::Mount{
              .destination = devPath,
              .options = string_list{ "rbind" },
              .source = devPath,
              .type = "bind",
            });
        }
    }

    // using FHS media directory and ignore '/run/media' for now
    // FIXME: the mount base location of udisks will be affected by the flag
    // '--enable-fhs-media', if not set this option, udisks will choose `/run/media` as the
    // mount location. some linux distros (e.g. ArchLinux) don't have this flag enabled, perhaps
    // we could find a better way to compatible with those distros.
    // https://github.com/storaged-project/udisks/commit/ae2a5ff1e49ae924605502ace170eb831e9c38e4
    if (std::filesystem::exists("/media")) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/media",
          .options = string_list{ "rbind", "rshared" },
          .source = "/media",
          .type = "bind",
        });
    }

    config.mounts = std::move(mounts);
    return true;
}

} // namespace linglong::generator
