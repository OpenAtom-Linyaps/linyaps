// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <tl/expected.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace linglong::common::display {

struct XOrgDisplayConf
{
    std::optional<std::string> protocol;
    std::optional<std::string> host;
    int displayNo;
    int screenNo;
};

tl::expected<std::filesystem::path, std::string>
getWaylandDisplay(std::string_view display) noexcept;
utils::error::Result<XOrgDisplayConf> getXOrgDisplay(std::string_view display) noexcept;
tl::expected<std::filesystem::path, std::string>
getXOrgAuthFile(std::string_view authFile) noexcept;

} // namespace linglong::common::display
