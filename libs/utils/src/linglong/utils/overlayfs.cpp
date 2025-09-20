// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "overlayfs.h"

#include "linglong/utils/command/cmd.h"

#include <QDebug>
#include <QString>

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
    auto res = utils::command::Cmd("fusermount")
                 .exec({ "-z", "-u", QString::fromStdString(merged_.string()) });
    if (!res) {
        qWarning() << QString("failed to umount %1 ").arg(QString::fromStdString(merged_.string()))
                   << res.error();
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

    utils::command::Cmd("fusermount")
      .exec({ "-z", "-u", QString::fromStdString(merged_.string()) });
    auto ret = utils::command::Cmd("fuse-overlayfs")
                 .exec({ "-o",
                         QString("lowerdir=%1,upperdir=%2,workdir=%3")
                           .arg((lowerdir_.c_str()), (upperdir_.c_str()), (workdir_.c_str())),
                         QString::fromStdString(merged_.string()) });
    if (!ret) {
        qWarning() << "failed to mount " << ret.error();
    }

    return !!ret;
}

void OverlayFS::unmount(bool clean)
{
    auto res = utils::command::Cmd("fusermount")
                 .exec({ "-z", "-u", QString::fromStdString(merged_.string()) });
    if (!res) {
        qWarning() << QString("failed to umount %1 ").arg(QString::fromStdString(merged_.string()))
                   << res.error();
    }

    if (clean) {
        std::error_code ec;
        std::filesystem::remove_all(upperdir_, ec);
        std::filesystem::remove_all(workdir_, ec);
    }
}

} // namespace linglong::utils
