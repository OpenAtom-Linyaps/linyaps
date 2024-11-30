/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace linglong {
void writeContainerJson(const std::filesystem::path &stateRoot,
                        const std::string &bundle,
                        const std::string &id,
                        pid_t pid);
nlohmann::json readAllContainerJson(const std::filesystem::path &stateRoot) noexcept;
}; // namespace linglong
