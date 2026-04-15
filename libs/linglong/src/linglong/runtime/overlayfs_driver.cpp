/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/overlayfs_driver.h"

#include "linglong/utils/file.h"

#include <fstream>
#include <regex>

namespace linglong::runtime {

auto OverlayFSDriver::create(utils::OverlayMode mode) noexcept -> std::unique_ptr<OverlayFSDriver>
{
    switch (mode) {
    case utils::OverlayMode::Kernel:
        return std::make_unique<OverlayFSKernelDriver>();
    case utils::OverlayMode::FUSE:
        return std::make_unique<OverlayFSFuseDriver>();
    case utils::OverlayMode::Auto:
        if (canUseKernelOverlay()) {
            return std::make_unique<OverlayFSKernelDriver>();
        }
        return std::make_unique<OverlayFSFuseDriver>();
    }

    return std::make_unique<OverlayFSFuseDriver>();
}

auto OverlayFSDriver::detectKernelOverlaySupport() noexcept -> OverlaySupport
{
    OverlaySupport support{ false, false, false };

    std::ifstream filesystems("/proc/filesystems");
    if (filesystems.is_open()) {
        std::string line;
        while (std::getline(filesystems, line)) {
            if (line.find("overlay") != std::string::npos) {
                support.kernelAvailable = true;
                break;
            }
        }
    }

    std::ifstream version("/proc/version");
    if (version.is_open()) {
        std::string line;
        std::getline(version, line);
        std::regex versionRegex("Linux version (\\d+)\\.(\\d+)");
        std::smatch match;
        if (std::regex_search(line, match, versionRegex) && match.size() > 2) {
            int major = std::stoi(match[1]);
            int minor = std::stoi(match[2]);
            if (major > 5 || (major == 5 && minor >= 11)) {
                support.kernelVersionOK = true;
            }
        }
    }

    support.canUseInUserNS = support.kernelAvailable && support.kernelVersionOK;
    return support;
}

auto OverlayFSDriver::canUseKernelOverlay() noexcept -> bool
{
    return detectKernelOverlaySupport().canUseInUserNS;
}

auto OverlayFSDriver::modeToString(utils::OverlayMode mode) noexcept -> std::string_view
{
    return utils::overlayModeToString(mode);
}

auto OverlayFSDriver::modeFromString(std::string_view mode) noexcept
  -> utils::error::Result<utils::OverlayMode>
{
    return utils::overlayModeFromString(mode);
}

auto OverlayFSDriver::createOverlayFS(const std::vector<std::filesystem::path> &lowerdirs,
                                      const std::filesystem::path &overlayInternal,
                                      const std::filesystem::path &bundlePath,
                                      bool persistent) noexcept
  -> utils::error::Result<std::unique_ptr<utils::OverlayFS>>
{
    LINGLONG_TRACE("create overlayfs");

    auto effectiveLowerdirs = lowerdirs;
    std::optional<std::filesystem::path> upperdir;
    std::optional<std::filesystem::path> workdir;

    auto result =
      prepare(effectiveLowerdirs, overlayInternal, bundlePath, persistent, upperdir, workdir);
    if (!result) {
        return LINGLONG_ERR("prepare overlayfs", result);
    }

    return std::make_unique<utils::OverlayFS>(std::move(effectiveLowerdirs),
                                              upperdir,
                                              workdir,
                                              bundlePath / "rootfs",
                                              mode());
}

auto OverlayFSDriver::prepare(std::vector<std::filesystem::path> &lowerdirs,
                              const std::filesystem::path &overlayInternal,
                              const std::filesystem::path &bundlePath,
                              bool persistent,
                              std::optional<std::filesystem::path> &upperdir,
                              std::optional<std::filesystem::path> &workdir) noexcept
  -> utils::error::Result<void>
{
    LINGLONG_TRACE("prepare overlayfs");

    auto overlayPath = overlayInternal;
    if (!persistent) {
        lowerdirs.insert(lowerdirs.begin(), overlayInternal / "upperdir");
        overlayPath = bundlePath / "overlay";
    }
    upperdir = overlayPath / "upperdir";
    workdir = overlayPath / "workdir";

    auto result = utils::ensureDirectory(*upperdir);
    if (!result) {
        return LINGLONG_ERR("ensure kernel overlayfs upperdir", result);
    }

    result = utils::ensureDirectory(*workdir);
    if (!result) {
        return LINGLONG_ERR("ensure kernel overlayfs workdir", result);
    }

    return LINGLONG_OK;
}

} // namespace linglong::runtime
