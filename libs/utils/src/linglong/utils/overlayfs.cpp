// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "overlayfs.h"

#include "linglong/utils/cmd.h"
#include "linglong/utils/log/log.h"

#include <filesystem>
#include <system_error>

namespace linglong::utils {

OverlayFS::OverlayFS(std::filesystem::path lowerdir,
                     std::filesystem::path upperdir,
                     std::filesystem::path workdir,
                     std::filesystem::path merged)
    : lowerdir_(std::move(lowerdir))
    , upperdir_(std::move(upperdir))
    , workdir_(std::move(workdir))
    , merged_(std::move(merged))
{
}

OverlayFS::~OverlayFS()
{
    auto res = utils::Cmd("fusermount").exec({ "-z", "-u", merged_.string() });
    if (!res) {
        LogW("failed to umount {}: {}", merged_.string(), res.error());
    }
}

bool OverlayFS::mount()
{
    std::error_code ec;
    std::filesystem::create_directories(upperdir_, ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(workdir_, ec);
    if (ec) {
        return false;
    }

    std::filesystem::create_directories(merged_, ec);
    if (ec) {
        return false;
    }

    // TODO: check mountpoint whether already mounted
    auto ret = utils::Cmd("fusermount").exec({ "-z", "-u", merged_.string() });
    if (!ret) {
        LogD("failed to set lazy umount {}", ret.error());
    }

    ret =
      utils::Cmd("fuse-overlayfs")
        .exec({ "-o",
                fmt::format("lowerdir={},upperdir={},workdir={}", lowerdir_, upperdir_, workdir_),
                merged_.string() });
    if (!ret) {
        LogW("failed to mount {}", ret.error());
    }

    return !!ret;
}

void OverlayFS::unmount(bool clean)
{
    auto res = utils::Cmd("fusermount").exec({ "-z", "-u", merged_.string() });
    if (!res) {
        LogW("failed to umount {}: {}", merged_.string(), res.error());
    }

    if (clean) {
        std::error_code ec;
        std::filesystem::remove_all(upperdir_, ec);
        std::filesystem::remove_all(workdir_, ec);
    }
}

} // namespace linglong::utils
