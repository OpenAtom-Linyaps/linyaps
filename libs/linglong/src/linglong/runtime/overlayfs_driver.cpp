/*
 * SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/runtime/overlayfs_driver.h"

#include "linglong/utils/cmd.h"
#include "linglong/utils/file.h"

#include <array>
#include <fstream>

#include <sys/utsname.h>

namespace linglong::runtime {
namespace {

auto filesystemHasOverlay() noexcept -> bool
{
    std::ifstream filesystems("/proc/filesystems");
    if (!filesystems.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(filesystems, line)) {
        if (line.find("overlay") != std::string::npos) {
            return true;
        }
    }

    return false;
}

auto modinfoHasOverlay() noexcept -> bool
{
    auto modinfo = utils::Cmd("modinfo");
    if (!modinfo.exists()) {
        return false;
    }

    return modinfo.exec({ "overlay" }).has_value();
}

auto overlayModuleExists() noexcept -> bool
{
    struct utsname uts{};
    if (uname(&uts) != 0) {
        return false;
    }

    const auto modulesRoot = std::filesystem::path("/lib/modules") / uts.release;
    const auto overlayModuleDir = modulesRoot / "kernel" / "fs" / "overlayfs";
    constexpr std::array<std::string_view, 4> overlayModules{
        "overlay.ko",
        "overlay.ko.gz",
        "overlay.ko.xz",
        "overlay.ko.zst",
    };

    std::error_code ec;
    for (const auto module : overlayModules) {
        if (std::filesystem::exists(overlayModuleDir / module, ec)) {
            return true;
        }
    }

    for (const auto &index : { "modules.dep", "modules.builtin" }) {
        std::ifstream modulesIndex(modulesRoot / index);
        if (!modulesIndex.is_open()) {
            continue;
        }

        std::string line;
        while (std::getline(modulesIndex, line)) {
            if (line.find("kernel/fs/overlayfs/overlay.ko") != std::string::npos) {
                return true;
            }
        }
    }

    return false;
}

auto kernelVersionSupportsUserNamespaceOverlay() noexcept -> bool
{
    struct utsname uts{};
    if (uname(&uts) != 0) {
        return false;
    }

    int major = 0;
    int minor = 0;
    if (std::sscanf(uts.release, "%d.%d", &major, &minor) == 2) {
        return major > 5 || (major == 5 && minor >= 11);
    }

    return false;
}

} // namespace

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

    support.kernelAvailable =
      filesystemHasOverlay() || modinfoHasOverlay() || overlayModuleExists();
    support.kernelVersionOK = kernelVersionSupportsUserNamespaceOverlay();

    support.canUseInUserNS = support.kernelAvailable && support.kernelVersionOK;
    return support;
}

auto OverlayFSDriver::canUseKernelOverlay() noexcept -> bool
{
    return detectKernelOverlaySupport().canUseInUserNS;
}

auto OverlayFSDriver::canUseFUSEOverlay() noexcept -> bool
{
    return utils::Cmd("fuse-overlayfs").exists();
}

auto OverlayFSDriver::resolveOverlayMode(utils::OverlayMode mode) noexcept
  -> utils::error::Result<utils::OverlayMode>
{
    LINGLONG_TRACE("resolve overlayfs mode");

    switch (mode) {
    case utils::OverlayMode::Kernel:
        if (canUseKernelOverlay()) {
            return utils::OverlayMode::Kernel;
        }
        return LINGLONG_ERR("kernel overlayfs is unavailable");
    case utils::OverlayMode::FUSE:
        if (canUseFUSEOverlay()) {
            return utils::OverlayMode::FUSE;
        }
        return LINGLONG_ERR("fuse-overlayfs command is unavailable");
    case utils::OverlayMode::Auto:
        if (canUseKernelOverlay()) {
            return utils::OverlayMode::Kernel;
        }
        if (canUseFUSEOverlay()) {
            return utils::OverlayMode::FUSE;
        }
        return LINGLONG_ERR("no available overlayfs implementation");
    }

    return LINGLONG_ERR("unknown overlayfs mode");
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
