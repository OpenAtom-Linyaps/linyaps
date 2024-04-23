/*
 * SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_CONTAINER_HELPER_H_
#define LINGLONG_BOX_CONTAINER_HELPER_H_
#include <nlohmann/json.hpp>
#include <string>

namespace linglong {
void writeContainerJson(const std::string &bundle, const std::string &id, pid_t pid);
nlohmann::json readAllContainerJson() noexcept;
}; // namespace linglong
#endif
