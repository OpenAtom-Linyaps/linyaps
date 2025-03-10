// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "00_id_mapping.h"

#include <iostream>

#include <unistd.h>

namespace linglong::generator {

bool IDMapping::generate(ocppi::runtime::config::types::Config &config) const noexcept
{
    if (config.ociVersion != "1.0.1") {
        std::cerr << "OCI version mismatched." << std::endl;
        return false;
    }

    auto linux_ = config.linux_.value_or(ocppi::runtime::config::types::Linux{});

    auto uidMappings = std::vector<ocppi::runtime::config::types::IdMapping>{
        ocppi::runtime::config::types::IdMapping{
          .containerID = ::getuid(),
          .hostID = ::getuid(),
          .size = 1,
        }
    };

    auto gidMappings = std::vector<ocppi::runtime::config::types::IdMapping>{
        ocppi::runtime::config::types::IdMapping{
          .containerID = ::getgid(),
          .hostID = ::getgid(),
          .size = 1,
        }
    };

    linux_.uidMappings = std::move(uidMappings);
    linux_.gidMappings = std::move(gidMappings);
    config.linux_ = std::move(linux_);

    return true;
}

} // namespace linglong::generator
