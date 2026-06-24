/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "hooks.h"

#include "cmd.h"
#include "configure.h"
#include "linglong/common/error.h"
#include "linglong/common/strings.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/log.h"

#include <fmt/format.h>

#include <array>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {

// Constants for hook action prefixes
constexpr std::string_view PRE_INSTALL_ACTION_PREFIX = "ll-pre-install=";
constexpr std::string_view POST_INSTALL_ACTION_PREFIX = "ll-post-install=";
constexpr std::string_view POST_UNINSTALL_ACTION_PREFIX = "ll-post-uninstall=";

using CommandList = const std::vector<std::string> &;

utils::error::Result<void> executeHookCommands(
  CommandList commands, const std::vector<std::pair<std::string, std::string>> &envVars) noexcept
{
    LINGLONG_TRACE("Executing hook commands");

    for (const auto &command : commands) {
        Cmd cmd("sh");

        for (const auto &[name, value] : envVars) {
            cmd.setEnv(name, value);
        }

        cmd.toStdin(command);
        auto result = cmd.exec({});
        if (!result.has_value()) {
            return LINGLONG_ERR(
              fmt::format("Hook command '{}' failed: {}.", command, result.error()));
        }
    }
    return LINGLONG_OK;
}

utils::error::Result<void> InstallHookManager::parseInstallHooks()
{
    LINGLONG_TRACE("Parsing install hooks");

    auto extractValue = [](const std::string &line, std::size_t prefixLen) -> std::string {
        auto trimmed = linglong::common::strings::trim(std::string_view(line).substr(prefixLen));
        if (trimmed.size() >= 2
            && ((trimmed.front() == '"' && trimmed.back() == '"')
                || (trimmed.front() == '\'' && trimmed.back() == '\''))) {
            trimmed.remove_prefix(1);
            trimmed.remove_suffix(1);
        }
        return std::string(trimmed);
    };

    std::error_code ec;
    for (const auto &entry : std::filesystem::directory_iterator(LINGLONG_INSTALL_HOOKS_DIR, ec)) {
        if (ec) {
            return LINGLONG_ERR(
              fmt::format("Failed to iterate directory {}", LINGLONG_INSTALL_HOOKS_DIR),
              ec);
        }

        if (!std::filesystem::is_regular_file(entry.status(ec))) {
            if (ec) {
                LogE("Failed to get status for {}: {}", entry.path().c_str(), ec.message());
            }
            continue;
        }

        std::ifstream file(entry.path());
        if (!file.is_open()) {
            return LINGLONG_ERR(fmt::format("Couldn't open file: {}", entry.path()));
        }

        std::string line;
        while (std::getline(file, line)) {
            std::size_t pos = line.find(PRE_INSTALL_ACTION_PREFIX);
            if (pos != std::string::npos) {
                preInstallCommands.emplace_back(
                  extractValue(line, pos + PRE_INSTALL_ACTION_PREFIX.length()));
                break;
            }

            pos = line.find(POST_INSTALL_ACTION_PREFIX);
            if (pos != std::string::npos) {
                postInstallCommands.emplace_back(
                  extractValue(line, pos + POST_INSTALL_ACTION_PREFIX.length()));
                break;
            }

            pos = line.find(POST_UNINSTALL_ACTION_PREFIX);
            if (pos != std::string::npos) {
                postUninstallCommands.emplace_back(
                  extractValue(line, pos + POST_UNINSTALL_ACTION_PREFIX.length()));
                break;
            }
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> InstallHookManager::executeInstallHooks(int fd) noexcept
{
    LINGLONG_TRACE("Executing pre-install hooks.");

    if (preInstallCommands.empty()) {
        return LINGLONG_OK;
    }

    // Convert fd into a specific path using /proc/pid/fd/fd_num
    std::ostringstream oss;
    oss << "/proc/" << getpid() << "/fd/" << fd;

    std::array<char, PATH_MAX + 1> pathBuf{};
    auto size = readlink(oss.str().c_str(), pathBuf.data(), PATH_MAX);

    if (size == -1) {
        return LINGLONG_ERR(fmt::format("Failed to read file link for fd {}: {}",
                                        fd,
                                        common::error::errorString(errno)));
    }

    pathBuf[size] = '\0';
    std::string uabPath = pathBuf.data();

    std::vector<std::pair<std::string, std::string>> envVars = { { "LINGLONG_UAB_PATH", uabPath } };

    return executeHookCommands(preInstallCommands, envVars);
}

utils::error::Result<void> InstallHookManager::executePostInstallHooks(
  const std::string &appID, const std::string &path) noexcept
{
    if (postInstallCommands.empty()) {
        return LINGLONG_OK;
    }

    const std::vector<std::pair<std::string, std::string>> envVars = {
        { "LINGLONG_APPID", appID },
        { "LINGLONG_APP_INSTALL_PATH", path }
    };

    return executeHookCommands(postInstallCommands, envVars);
}

utils::error::Result<void>
InstallHookManager::executePostUninstallHooks(const std::string &appID) noexcept
{
    if (postUninstallCommands.empty()) {
        return LINGLONG_OK;
    }

    const std::vector<std::pair<std::string, std::string>> envVars = { { "LINGLONG_APPID",
                                                                         appID } };

    return executeHookCommands(postUninstallCommands, envVars);
}

} // namespace linglong::utils
