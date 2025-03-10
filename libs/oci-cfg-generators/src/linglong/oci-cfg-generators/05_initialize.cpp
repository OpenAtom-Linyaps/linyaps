// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "05_initialize.h"

#include <filesystem>
#include <iostream>

namespace linglong::generator {

bool Initialize::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (config.ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return false;
    }

    if (!config.annotations) {
        std::cerr << "no annotations." << std::endl;
        return false;
    }
    const auto &annotations = config.annotations.value();

    auto appID = config.annotations->find("org.deepin.linglong.appID");
    if (appID == config.annotations->end()) {
        std::cerr << "appID not found." << std::endl;
        return false;
    }

    if (appID->second.empty()) {
        std::cerr << "appID is empty." << std::endl;
        return false;
    }

    auto mounts = config.mounts.value_or(std::vector<ocppi::runtime::config::types::Mount>{});

    if (auto runtimeDir = annotations.find("org.deepin.linglong.runtimeDir");
        runtimeDir != annotations.end()) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/runtime",
          .options = string_list{ "rbind", "ro" },
          .source = std::filesystem::path(runtimeDir->second) / "files",
          .type = "bind" });
    }

    if (auto appDir = annotations.find("org.deepin.linglong.appDir"); appDir != annotations.end()) {
        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = "/opt",
          .options = string_list{ "nodev", "nosuid", "mode=700" },
          .source = "tmpfs",
          .type = "tmpfs",
        });

        mounts.push_back(ocppi::runtime::config::types::Mount{
          .destination = std::filesystem::path("/opt/apps") / appID->second / "files",
          .options = string_list{ "rbind", "rw" },
          .source = std::filesystem::path(appDir->second) / "files",
          .type = "bind" });
    }

    config.mounts = std::move(mounts);

    return true;
}
} // namespace linglong::generator
