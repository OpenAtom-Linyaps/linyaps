/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "container/helper.h"

#include "ocppi/types/Generators.hpp"
#include "util/logger.h"

#include <filesystem>

namespace linglong {
void writeContainerJson(const std::filesystem::path &stateRoot,
                        const std::string &bundle,
                        const std::string &id,
                        pid_t pid)
{
    ocppi::types::ContainerListItem item = {
        .bundle = bundle,
        .id = id,
        .pid = pid,
        .status = "running",
    };

    std::filesystem::create_directories(stateRoot);
    if (!std::filesystem::exists(stateRoot)) {
        logErr() << "create_directories" << stateRoot << "failed";
        assert(false);
    }

    std::ofstream file(stateRoot / (id + ".json"));
    if (file.is_open()) {
        file << nlohmann::json(item).dump(4);
    } else {
        logErr() << "open" << stateRoot / (id + ".json") << "failed";
        assert(false);
    }
}

nlohmann::json readAllContainerJson(const std::filesystem::path &stateRoot) noexcept
{
    nlohmann::json result = nlohmann::json::array();

    std::error_code ec;
    std::filesystem::create_directories(stateRoot, ec);
    if (ec) {
        logErr() << "failed to create" << stateRoot.string() << ec.message();
        return {};
    }

    for (const auto &entry : std::filesystem::directory_iterator{ stateRoot }) {
        std::ifstream containerInfo = entry.path();
        if (!containerInfo.is_open()) {
            continue;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(containerInfo);
            result.push_back(j);
        } catch (const std::exception &e) {
            logErr() << "parse" << entry.path() << "failed" << e.what();
        }
    }

    return result;
}
} // namespace linglong
