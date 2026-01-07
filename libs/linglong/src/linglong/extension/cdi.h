/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/DeviceNode.hpp"
#include "linglong/utils/error/error.h"
#include "ocppi/runtime/config/types/Mount.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace linglong::extension::cdi {

struct ContainerEdits
{
    std::map<std::string, std::string> env;
    std::vector<ocppi::runtime::config::types::Mount> mounts;
    std::vector<api::types::v1::DeviceNode> deviceNodes;

    bool empty() const noexcept
    {
        return env.empty() && mounts.empty() && deviceNodes.empty();
    }
};

utils::error::Result<ContainerEdits> loadFromFile(const std::filesystem::path &path) noexcept;
utils::error::Result<ContainerEdits> loadFromJson(const std::string &jsonText) noexcept;
utils::error::Result<ContainerEdits> loadFromNvidiaCtk() noexcept;

// Built-in NVIDIA CDI-like discovery without relying on nvidia-ctk.
// Intended as a fallback for `ll-cli extension import-cdi`.
utils::error::Result<ContainerEdits> loadFromHostNvidia() noexcept;

} // namespace linglong::extension::cdi
