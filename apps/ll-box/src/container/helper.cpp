/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/helper.h"

#include "ocppi/types/Generators.hpp"
#include "util/logger.h"

namespace linglong {
void writeContainerJson(const std::string &bundle, const std::string &id, pid_t pid)
{
    ocppi::types::ContainerListItem item = {
        .bundle = bundle,
        .id = id,
        .pid = pid,
        .status = "running",
    };

    auto dir =
      std::filesystem::path("/run") / "user" / std::to_string(getuid()) / "linglong" / "box";
    std::filesystem::create_directories(dir);
    if (!std::filesystem::exists(dir)) {
        logErr() << "create_directories" << dir << "failed";
        assert(false);
    }

    std::ofstream file(dir / (id + ".json"));
    if (file.is_open()) {
        file << nlohmann::json(item).dump(4);
    } else {
        logErr() << "open" << dir / (id + ".json") << "failed";
        assert(false);
    }
}
} // namespace linglong
