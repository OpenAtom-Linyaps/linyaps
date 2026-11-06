// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <sys/types.h>

namespace linglong::utils::runtime_directory {

class RuntimeLayout
{
public:
    static RuntimeLayout forUser(std::filesystem::path root) noexcept;
    static RuntimeLayout forPrivilegedHost(std::filesystem::path root) noexcept;
    static utils::error::Result<RuntimeLayout> current() noexcept;

    [[nodiscard]] const std::filesystem::path &root() const noexcept;
    [[nodiscard]] std::filesystem::path runtimeDir() const noexcept;
    [[nodiscard]] bool isPrivilegedHost() const noexcept;

    [[nodiscard]] std::filesystem::path
    bundleDir(std::string_view containerID, std::string_view bundleSuffix = "") const noexcept;
    [[nodiscard]] std::filesystem::path appRuntimeDir(const std::string &appId) const noexcept;
    [[nodiscard]] std::filesystem::path containerStateDir() const noexcept;
    [[nodiscard]] std::filesystem::path consoleSocket(std::string_view name) const noexcept;

    utils::error::Result<void> prepareRuntimeDir() const noexcept;
    utils::error::Result<std::filesystem::path>
    prepareAppRuntimeDir(const std::string &appId) const noexcept;

private:
    RuntimeLayout(std::filesystem::path root, bool privilegedHost) noexcept;

    std::filesystem::path root_;
    bool privilegedHost_;
};

utils::error::Result<void> validateAppId(const std::string &appId) noexcept;

utils::error::Result<std::filesystem::path> preparePrivilegedAppRuntimeDir(
  const std::filesystem::path &runtimeDir, const std::string &appId, uid_t owner) noexcept;

} // namespace linglong::utils::runtime_directory
