/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/utils.h"

#include "linglong/utils/error/error.h"

#include <algorithm>
#include <string_view>

namespace linglong::package {

utils::error::Result<void> validateExecutableName(std::string_view name) noexcept
{
    LINGLONG_TRACE("validate executable name");

    if (name.empty()) {
        return LINGLONG_ERR("executable name must not be empty");
    }

    if (name.size() > 255) {
        return LINGLONG_ERR("executable name length must not exceed 255");
    }

    if (name == "." || name == "..") {
        return LINGLONG_ERR("executable name must not be \".\" or \"..\"");
    }

    if (name.find('/') != std::string_view::npos) {
        return LINGLONG_ERR("executable name must not contain '/'");
    }

    if (name.find('\0') != std::string_view::npos) {
        return LINGLONG_ERR("executable name must not contain NUL");
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
