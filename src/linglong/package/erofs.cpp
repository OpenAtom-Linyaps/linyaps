/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/erofs.h"

#include "linglong/util/runner.h"

namespace linglong::package {
Erofs::Erofs() { }

Erofs::~Erofs() { }

linglong::utils::error::Result<void> Erofs::mount(const QString &erofsPath,
                                                  const QString &mountPoint,
                                                  const longlong &offset = 0)
{ // TODO: support more backend of erofs
    auto ret =
      util::Exec("erofsfuse", { QString("--offset=%1").arg(offset), erofsPath, mountPoint });
    if (ret) {
        return LINGLONG_ERR(ret.code(), "call mount.erofs failed")
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> compress(const QString &srcDir, const QString &destFilePath)
{
    auto ret = util::Exec("mkfs.erofs",
                          { "-zlz4hc,9", this->erofsFilePath, this->bundleDataPath },
                          15 * 60 * 1000);
    if (ret) {
        return LINGLONG_ERR(ret.code(), "call mkfs.erofs failed");
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> decompress(const QString &targetFilePath,
                                                const QString &destPath,
                                                const longlong offset)
{
    auto ret = mount(erofsPath, destPath, offset);
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("failed to decompress file", ret.error());
    }

    return LINGLONG_OK;
}

} // namespace linglong::package