// SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "overlayfs.h"

#include "linglong/common/error.h"
#include "linglong/common/strings.h"
#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"

#include <sys/mount.h>

namespace linglong::utils {

auto overlayModeToString(OverlayMode mode) noexcept -> std::string_view
{
    switch (mode) {
    case OverlayMode::Auto:
        return "auto";
    case OverlayMode::Kernel:
        return "kernel";
    case OverlayMode::FUSE:
        return "fuse";
    }

    return "unknown";
}

auto overlayModeFromString(std::string_view mode) noexcept -> utils::error::Result<OverlayMode>
{
    LINGLONG_TRACE("parse overlayfs mode");

    if (mode == "auto") {
        return OverlayMode::Auto;
    }
    if (mode == "kernel") {
        return OverlayMode::Kernel;
    }
    if (mode == "fuse") {
        return OverlayMode::FUSE;
    }

    return LINGLONG_ERR(fmt::format("invalid overlayfs mode: {}", mode));
}

OverlayFS::OverlayFS(std::vector<std::filesystem::path> lowerdirs,
                     std::optional<std::filesystem::path> upperdir,
                     std::optional<std::filesystem::path> workdir,
                     std::filesystem::path merged,
                     OverlayMode mode)
    : lowerdirs_(std::move(lowerdirs))
    , upperdir_(std::move(upperdir))
    , workdir_(std::move(workdir))
    , merged_(std::move(merged))
    , mounted_(false)
    , mode_(mode)
{
}

OverlayFS::~OverlayFS()
{
    unmount();
}

bool OverlayFS::mount()
{
    if (mounted_) {
        LogW("OverlayFS already mounted at {}", merged_.string());
        return false;
    }

    bool success = false;
    switch (mode_) {
    case OverlayMode::Kernel:
        success = mountKernelOverlay();
        break;
    case OverlayMode::FUSE:
        success = mountFUSEOverlay();
        break;
    case OverlayMode::Auto:
        LogW("overlayfs mode must be determined before mount");
        return false;
    }

    if (success) {
        mounted_ = true;
        LogD("OverlayFS mounted successfully at {} using mode: {}",
             merged_.string(),
             overlayModeToString(mode_));
    } else {
        LogW("OverlayFS mount failed at {}", merged_.string());
    }

    return success;
}

void OverlayFS::unmount()
{
    if (!mounted_) {
        return;
    }

    if (mode_ == OverlayMode::Kernel) {
        if (umount(merged_.c_str()) != 0) {
            if (errno != EINVAL && errno != ENOENT) {
                if (umount2(merged_.c_str(), MNT_DETACH) != 0) {
                    LogW("failed to umount {}: {}",
                         merged_.string(),
                         common::error::errorString(errno));
                }
            }
        } else {
            LogD("Kernel overlay umounted: {}", merged_.string());
        }
    } else {
        auto res = utils::Cmd("fusermount").exec({ "-z", "-u", merged_.string() });
        if (!res) {
            LogW("failed to umount {}: {}", merged_.string(), res.error());
        }
    }

    mounted_ = false;
}

bool OverlayFS::mountKernelOverlay()
{
    std::string options;
    if (upperdir_ && workdir_) {
        options = fmt::format("lowerdir={},upperdir={},workdir={},userxattr",
                              lowerdirOption(),
                              upperdir_->string(),
                              workdir_->string());
    } else {
        options = fmt::format("lowerdir={}", lowerdirOption());
    }

    LogD("mounting kernel overlay with options: {}", options);

    if (::mount("none", merged_.c_str(), "overlay", 0, options.c_str()) != 0) {
        LogW("kernel overlay mount failed: {}", common::error::errorString(errno));
        return false;
    }

    return true;
}

bool OverlayFS::mountFUSEOverlay()
{
    std::string options;
    if (upperdir_ && workdir_) {
        options = fmt::format("lowerdir={},upperdir={},workdir={}",
                              lowerdirOption(),
                              upperdir_->string(),
                              workdir_->string());
    } else {
        options = fmt::format("lowerdir={}", lowerdirOption());
    }

    auto res = utils::Cmd("fuse-overlayfs").exec({ "-o", options, merged_.string() });
    if (!res) {
        LogW("fuse-overlayfs mount failed: {}", res.error());
    }
    return !!res;
}

std::string OverlayFS::lowerdirOption()
{
    std::string lowerdirs;
    for (size_t i = 0; i < lowerdirs_.size(); ++i) {
        if (i > 0) {
            lowerdirs += ":";
        }
        auto escaped = common::strings::replaceSubstring(lowerdirs_[i].string(), ":", "\\:");
        escaped = common::strings::replaceSubstring(escaped, ",", "\\,");
        lowerdirs += escaped;
    }
    return lowerdirs;
}

} // namespace linglong::utils
