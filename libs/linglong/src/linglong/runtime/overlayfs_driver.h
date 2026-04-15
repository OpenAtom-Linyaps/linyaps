/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/utils/error/error.h"
#include "linglong/utils/overlayfs.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace linglong::runtime {

struct OverlaySupport
{
    bool kernelAvailable;
    bool kernelVersionOK;
    bool canUseInUserNS;
};

class OverlayFSDriver
{
public:
    explicit OverlayFSDriver(utils::OverlayMode mode) noexcept
        : mode_(mode)
    {
    }

    virtual ~OverlayFSDriver() = default;

    static auto create(utils::OverlayMode mode) noexcept -> std::unique_ptr<OverlayFSDriver>;
    static auto detectKernelOverlaySupport() noexcept -> OverlaySupport;
    static auto canUseKernelOverlay() noexcept -> bool;
    static auto modeToString(utils::OverlayMode mode) noexcept -> std::string_view;
    static auto modeFromString(std::string_view mode) noexcept
      -> utils::error::Result<utils::OverlayMode>;

    auto createOverlayFS(const std::vector<std::filesystem::path> &lowerdirs,
                         const std::filesystem::path &overlayInternal,
                         const std::filesystem::path &bundlePath,
                         bool persistent) noexcept
      -> utils::error::Result<std::unique_ptr<utils::OverlayFS>>;

    utils::OverlayMode mode() const noexcept { return mode_; }

protected:
    auto prepare(std::vector<std::filesystem::path> &lowerdirs,
                 const std::filesystem::path &overlayInternal,
                 const std::filesystem::path &bundlePath,
                 bool persistent,
                 std::optional<std::filesystem::path> &upperdir,
                 std::optional<std::filesystem::path> &workdir) noexcept
      -> utils::error::Result<void>;

private:
    utils::OverlayMode mode_;
};

class OverlayFSKernelDriver : public OverlayFSDriver
{
public:
    OverlayFSKernelDriver() noexcept
        : OverlayFSDriver(utils::OverlayMode::Kernel)
    {
    }
};

class OverlayFSFuseDriver : public OverlayFSDriver
{
public:
    OverlayFSFuseDriver() noexcept
        : OverlayFSDriver(utils::OverlayMode::FUSE)
    {
    }
};

} // namespace linglong::runtime
