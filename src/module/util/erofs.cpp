/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "erofs.h"

#include "runner.h"

namespace linglong {
namespace erofs {

// TODO: use erofs-utils for now, change to erofs loop mount with kernel version > 5.19
util::Error mount(const QString &src, const QString &mountPoint)
{
    return util::Exec("erofsfuse", { src, mountPoint });
}

util::Error mkfs(const QString &srcDir, const QString &destImagePath)
{
    return util::Exec("mkfs.erofs", { "-zlz4", destImagePath, srcDir });
}

} // namespace erofs
} // namespace linglong