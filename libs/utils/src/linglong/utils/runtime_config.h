/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#pragma once

#include "linglong/api/types/v1/RuntimeConfigure.hpp"
#include "linglong/utils/error/error.h"

#include <filesystem>

namespace linglong::utils {

linglong::api::types::v1::RuntimeConfigure
MergeRuntimeConfig(const std::vector<linglong::api::types::v1::RuntimeConfigure> &configs);
utils::error::Result<linglong::api::types::v1::RuntimeConfigure>
loadRuntimeConfig(const std::filesystem::path &path);
utils::error::Result<std::optional<linglong::api::types::v1::RuntimeConfigure>>
loadRuntimeConfig(const std::string &appId);

} // namespace linglong::utils
