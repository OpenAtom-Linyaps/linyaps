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
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace linglong::utils {

// Constants for hook action prefixes
constexpr std::string_view PRE_INSTALL_ACTION_PREFIX = "ll-pre-install=";
constexpr std::string_view POST_INSTALL_ACTION_PREFIX = "ll-post-install=";
constexpr std::string_view POST_UNINSTALL_ACTION_PREFIX = "ll-post-uninstall=";

using CommandList = const std::vector<std::string> &;

namespace {

constexpr std::string_view WHITESPACE_CHARS = " \t\n\r\f\v";

} // namespace

utils::error::Result<std::optional<std::string>>
details::parseInstallHookCommandLine(std::string_view line, std::string_view prefix)
{
    LINGLONG_TRACE("Parsing install hook command line");

    line = common::strings::trim_left(line, WHITESPACE_CHARS);
    if (!common::strings::starts_with(line, prefix)) {
        return std::optional<std::string>{};
    }

    auto command = common::strings::trim_left(line.substr(prefix.size()), WHITESPACE_CHARS);
    if (command.empty()) {
        return std::optional<std::string>{ std::string{} };
    }

    const auto quote = command.front();
    if (quote != '"' && quote != '\'') {
        return std::optional<std::string>{ std::string(command) };
    }

    command.remove_prefix(1);

    auto suffix = common::strings::trim_right(command, WHITESPACE_CHARS);
    if (suffix.empty() || suffix.back() != quote) {
        return LINGLONG_ERR("Invalid install hook command: unterminated quoted command");
    }

    suffix.remove_suffix(1);
    return std::optional<std::string>{ std::string(suffix) };
}

utils::error::Result<void> executeHookCommands(
  CommandList commands, const std::vector<std::pair<std::string, std::string>> &envVars) noexcept
{
    LINGLONG_TRACE("Executing hook commands");

    for (const auto &command : commands) {
        Cmd cmd("sh");

        for (const auto &[name, value] : envVars) {
            cmd.setEnv(name, value);
        }

        auto result = cmd.exec({ "-c", command });
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

        const std::array<std::pair<std::string_view, std::vector<std::string> *>, 3> hookRules = {
            { { PRE_INSTALL_ACTION_PREFIX, &preInstallCommands },
              { POST_INSTALL_ACTION_PREFIX, &postInstallCommands },
              { POST_UNINSTALL_ACTION_PREFIX, &postUninstallCommands } }
        };

        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(file, line)) {
            ++lineNumber;

            for (auto [prefix, commands] : hookRules) {
                auto command = details::parseInstallHookCommandLine(line, prefix);
                if (!command.has_value()) {
                    return LINGLONG_ERR(fmt::format("Invalid install hook command in {}:{}",
                                                    entry.path().string(),
                                                    lineNumber),
                                        command);
                }

                if (!command->has_value()) {
                    continue;
                }

                commands->emplace_back(std::move(**command));
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
