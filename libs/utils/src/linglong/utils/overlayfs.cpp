// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "overlayfs.h"

#include <QDir>

#include "linglong/utils/command/env.h"

namespace linglong::utils {

OverlayFS::OverlayFS(QString lowerdir, QString upperdir, QString workdir, QString merged) :
    lowerdir_(lowerdir),
    upperdir_(upperdir),
    workdir_(workdir),
    merged_(merged)
{
}

OverlayFS::~OverlayFS()
{
    auto res = utils::command::Exec("umount", QStringList() << merged_);
    if (!res) {
        qWarning() << QString("failed to umount %1 ").arg(merged_) << res.error();
    }
}

bool OverlayFS::mount()
{
    QDir upperDir(upperdir_);
    if (!upperDir.mkpath(".")) {
        return false;
    }

    QDir workDir(workdir_);
    if (!workDir.mkpath(".")) {
        return false;
    }

    QDir mergedDir(merged_);
    if (!mergedDir.mkpath(".")) {
        return false;
    }

    utils::command::Exec("umount", QStringList() << merged_);

    auto ret = utils::command::Exec("fuse-overlayfs",
            QStringList() <<
            "fuse-overlayfs" << "-o" << QString("lowerdir=%1,upperdir=%2,workdir=%3")
            .arg(lowerdir_)
            .arg(upperdir_)
            .arg(workdir_) << merged_);
    if (!ret) {
        qWarning() << "failed to mount " << ret.error();
    }

    return !!ret;
}

void OverlayFS::unmount(bool clean)
{
    auto res = utils::command::Exec("umount", QStringList() << merged_);
    if (!res) {
        qWarning() << QString("failed to umount %1 ").arg(merged_) << res.error();
    }

    if (clean) {
        QDir(upperdir_).removeRecursively();
        QDir(workdir_).removeRecursively();
    }
}

}
