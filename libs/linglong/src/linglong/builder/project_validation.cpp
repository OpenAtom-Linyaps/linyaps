// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "project_validation.h"

#include "linglong/utils/xdp.h"

#include <fmt/format.h>

#include <algorithm>

namespace linglong::builder {

auto validatePackageId(std::string_view packageId) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("validate package.id");

    if (packageId.empty()) {
        return LINGLONG_ERR("package.id must not be empty");
    }

    if (!utils::isValidXdgDesktopPortalId(packageId)) {
        return LINGLONG_ERR(
          fmt::format("invalid package.id '{}': not a valid XDG desktop portal application ID",
                      packageId));
    }

    return LINGLONG_OK;
}

auto validateModuleName(std::string_view moduleName) noexcept -> utils::error::Result<void>
{
    LINGLONG_TRACE("validate module name");

    if (moduleName.empty()) {
        return LINGLONG_ERR("module name must not be empty");
    }

    const auto isAllowed = [](char c) {
        const bool isAlpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        const bool isDigit = c >= '0' && c <= '9';
        return isAlpha || isDigit || c == '_' || c == '-';
    };

    if (!std::all_of(moduleName.begin(), moduleName.end(), isAllowed)) {
        return LINGLONG_ERR(
          fmt::format("invalid module name '{}': only letters, digits, underscores and hyphens "
                      "are allowed",
                      moduleName));
    }

    return LINGLONG_OK;
}

} // namespace linglong::builder
