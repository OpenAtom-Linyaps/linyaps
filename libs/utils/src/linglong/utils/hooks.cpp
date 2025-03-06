/*
 * SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "hooks.h"

#include "linglong/utils/configure.h"
#include "linglong/utils/error/error.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include <sys/wait.h>

namespace linglong::utils {

static const std::string preInstallAction = "ll-pre-install=";
static const std::string postInstallAction = "ll-post-install=";
static const std::string postUninstallAction = "ll-post-uninstall=";

utils::error::Result<void> InstallHookManager::parseInstallHooks()
{
    LINGLONG_TRACE("parse install hooks");

    for (const auto &entry : std::filesystem::directory_iterator(LINGLONG_INSTALL_HOOKS_DIR)) {
        if (std::filesystem::is_regular_file(entry.path())) {
            std::ifstream file = entry.path();
            if (!file.is_open()) {
                return LINGLONG_ERR(QString{ "couldn't open file: %1" }.arg(entry.path().c_str()));
            }

            std::string line;
            while (!file.eof()) {
                std::getline(file, line);
                std::size_t pos = line.find(preInstallAction);
                if (pos != std::string::npos) {
                    preInstallCommands.emplace_back(line.substr(pos + preInstallAction.length()));
                    break;
                }

                pos = line.find(postInstallAction);
                if (pos != std::string::npos) {
                    postInstallCommands.emplace_back(line.substr(pos + postInstallAction.length()));
                    break;
                }

                pos = line.find(postUninstallAction);
                if (pos != std::string::npos) {
                    postUninstallCommands.emplace_back(
                      line.substr(pos + postUninstallAction.length()));
                    break;
                }
            }

            file.close();
        }
    }

    return LINGLONG_OK;
}

utils::error::Result<void> InstallHookManager::executeInstallHooks(int fd)
{
    LINGLONG_TRACE("execute install hooks");

    if (preInstallCommands.empty()) {
        return LINGLONG_OK;
    }

    // Convert fd into a specific path and pass it to the signature verification tool
    std::ostringstream oss;
    oss << "/proc/" << getpid() << "/fd/" << fd;
    std::array<char, PATH_MAX + 1> path{};
    auto size = readlink(oss.str().c_str(), path.data(), PATH_MAX);
    if (size == -1) {
        return LINGLONG_ERR(QString{ "failed to read file link: %1" }.arg(strerror(errno)));
    }

    if (-1 == setenv("LINGLONG_UAB_PATH", std::string(path.cbegin(), path.cend()).c_str(), 1)) {
        qWarning() << "failed to set LINGLONG_UAB_PATH" << errno;
    }

    for (const auto &command : preInstallCommands) {
        auto ret = std::system(command.c_str());
        if (!WIFEXITED(ret)) {
            int exitStatus = WEXITSTATUS(ret);
            return LINGLONG_ERR(
              QString("command did not terminate normally: %1.").arg(strerror(exitStatus)));
        }
    }

    if (::getenv("LINGLONG_UAB_PATH") != nullptr) {
        if (-1 == ::unsetenv("LINGLONG_UAB_PATH")) {
            qWarning() << "failed to unset LINGLONG_UAB_PATH" << errno;
        };
    }

    return LINGLONG_OK;
}

utils::error::Result<void> InstallHookManager::executePostInstallHooks(
  const std::string &appID, const std::string &path) noexcept
{
    LINGLONG_TRACE("execute post install hooks");

    if (postInstallCommands.empty()) {
        return LINGLONG_OK;
    }

    if (-1 == setenv("LINGLONG_APPID", std::string(appID.cbegin(), appID.cend()).c_str(), 1)) {
        qWarning() << "failed to set LINGLONG_APPID" << errno;
    }

    if (-1 == setenv("LINGLONG_APP_INSTALL_PATH", path.c_str(), 1)) {
        qWarning() << "failed to set LINGLONG_APP_INSTALL_PATH" << errno;
    }

    for (const auto &command : postInstallCommands) {
        auto ret = std::system(command.c_str());
        if (!WIFEXITED(ret)) {
            int exitStatus = WEXITSTATUS(ret);
            return LINGLONG_ERR(
              QString("command did not terminate normally: %1.").arg(strerror(exitStatus)));
        }
    }

    if (::getenv("LINGLONG_APPID") != nullptr) {
        if (-1 == ::unsetenv("LINGLONG_APPID")) {
            qWarning() << "failed to unset LINGLONG_APPID" << errno;
        };
    }

    if (::getenv("LINGLONG_APP_INSTALL_PATH") != nullptr) {
        if (-1 == ::unsetenv("LINGLONG_APP_INSTALL_PATH")) {
            qWarning() << "failed to unset LINGLONG_APP_INSTALL_PATH" << errno;
        };
    }

    return LINGLONG_OK;
}

utils::error::Result<void>
InstallHookManager::executePostUninstallHooks(const std::string &appID) noexcept
{
    LINGLONG_TRACE("execute post install hooks");

    if (postUninstallCommands.empty()) {
        return LINGLONG_OK;
    }

    if (-1 == setenv("LINGLONG_APPID", std::string(appID.cbegin(), appID.cend()).c_str(), 1)) {
        qWarning() << "failed to set LINGLONG_APPID" << errno;
    }

    for (const auto &command : postUninstallCommands) {
        auto ret = std::system(command.c_str());
        if (!WIFEXITED(ret)) {
            int exitStatus = WEXITSTATUS(ret);
            return LINGLONG_ERR(
              QString("command did not terminate normally: %1.").arg(strerror(exitStatus)));
        }
    }

    if (::getenv("LINGLONG_APPID") != nullptr) {
        if (-1 == ::unsetenv("LINGLONG_APPID")) {
            qWarning() << "failed to unset LINGLONG_APPID" << errno;
        };
    }

    return LINGLONG_OK;
}

} // namespace linglong::utils
