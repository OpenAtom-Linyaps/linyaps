/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <string>
#include <vector>

namespace linglong::utils {

class InstallHookManager final
{
public:
    InstallHookManager() = default;
    InstallHookManager(const InstallHookManager &) = delete;
    InstallHookManager(InstallHookManager &&) = delete;
    InstallHookManager &operator=(const InstallHookManager &) = delete;
    InstallHookManager &operator=(InstallHookManager &&) = delete;
    ~InstallHookManager() = default;

    utils::error::Result<void> parseInstallHooks();
    utils::error::Result<void> executeInstallHooks(int fd) noexcept;
    utils::error::Result<void> executePostInstallHooks(const std::string &appID,
                                                       const std::string &path) noexcept;
    utils::error::Result<void> executePostUninstallHooks(const std::string &appID) noexcept;

private:
    std::vector<std::string> preInstallCommands;
    std::vector<std::string> postInstallCommands;
    std::vector<std::string> postUninstallCommands;
};

} // namespace linglong::utils
