/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace linglong::utils {

namespace details {

utils::error::Result<std::optional<std::string>>
parseInstallHookCommandLine(std::string_view line, std::string_view prefix);

} // namespace details

class InstallHookManager final
{
public:
    explicit InstallHookManager(std::filesystem::path hooksDir = {});
    InstallHookManager(const InstallHookManager &) = delete;
    InstallHookManager(InstallHookManager &&) = delete;
    InstallHookManager &operator=(const InstallHookManager &) = delete;
    InstallHookManager &operator=(InstallHookManager &&) = delete;
    ~InstallHookManager() = default;

    utils::error::Result<void> parseInstallHooks();
    utils::error::Result<void> executeInstallHooks(const std::filesystem::path &path) noexcept;
    utils::error::Result<void> executePostInstallHooks(const std::string &appID,
                                                       const std::string &path) noexcept;
    utils::error::Result<void> executePostUninstallHooks(const std::string &appID) noexcept;

private:
    std::filesystem::path hooksDir_;
    std::vector<std::string> preInstallCommands;
    std::vector<std::string> postInstallCommands;
    std::vector<std::string> postUninstallCommands;
};

} // namespace linglong::utils
