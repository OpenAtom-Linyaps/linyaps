// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <string_view>

namespace linglong::builder {

auto validatePackageId(std::string_view packageId) noexcept -> utils::error::Result<void>;

auto validateModuleName(std::string_view moduleName) noexcept -> utils::error::Result<void>;

} // namespace linglong::builder
