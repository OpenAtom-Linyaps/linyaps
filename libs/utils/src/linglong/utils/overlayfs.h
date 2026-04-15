// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/utils/error/error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace linglong::utils {

enum class OverlayMode { Auto, Kernel, FUSE };

auto overlayModeToString(OverlayMode mode) noexcept -> std::string_view;
auto overlayModeFromString(std::string_view mode) noexcept -> utils::error::Result<OverlayMode>;

class OverlayFS
{
public:
    OverlayFS() = delete;
    OverlayFS(const OverlayFS &) = delete;
    OverlayFS &operator=(const OverlayFS &) = delete;

    OverlayFS(std::vector<std::filesystem::path> lowerdirs,
              std::optional<std::filesystem::path> upperdir,
              std::optional<std::filesystem::path> workdir,
              std::filesystem::path merged,
              OverlayMode mode);
    ~OverlayFS();

    bool mount();
    void unmount();

    std::optional<std::filesystem::path> upperDirPath() { return upperdir_; }

    std::optional<std::filesystem::path> workDirPath() { return workdir_; }

    const std::vector<std::filesystem::path> &lowerDirPaths() const { return lowerdirs_; }

    std::filesystem::path mergedDirPath() { return merged_; }

    bool isMounted() const { return mounted_; }

    OverlayMode getMode() const { return mode_; }

private:
    bool mountKernelOverlay();

    bool mountFUSEOverlay();

    std::string lowerdirOption();

    std::vector<std::filesystem::path> lowerdirs_;
    std::optional<std::filesystem::path> upperdir_;
    std::optional<std::filesystem::path> workdir_;
    std::filesystem::path merged_;
    bool mounted_;
    OverlayMode mode_;
};

} // namespace linglong::utils
