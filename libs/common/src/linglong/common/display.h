// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <tl/expected.hpp>

#include <filesystem>

namespace linglong::common {

tl::expected<std::filesystem::path, std::string>
getWaylandDisplay(std::string_view display) noexcept;
tl::expected<std::filesystem::path, std::string> getXOrgDisplay(std::string_view display) noexcept;
tl::expected<std::filesystem::path, std::string>
getXOrgAuthFile(std::string_view authFile) noexcept;

} // namespace linglong::common
