// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/CdiDeviceEntry.hpp"
#include "linglong/api/types/v1/RuntimeConfigure.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace linglong::cli {
struct RunOptions;
}

namespace linglong::runtime {

struct CdiDriverGeneration
{
    std::vector<api::types::v1::Device> devices;
    std::filesystem::path specDir;
};

CdiDriverGeneration loadCdiDriverGeneration(const std::filesystem::path &runtimeDir);

utils::error::Result<std::optional<std::vector<api::types::v1::CdiDeviceEntry>>>
resolveCdiDevices(const CdiDriverGeneration &generation,
                  const std::optional<api::types::v1::RuntimeConfigure> &configured,
                  const cli::RunOptions &options);

} // namespace linglong::runtime
