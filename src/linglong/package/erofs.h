/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_EROFS_H_
#define LINGLONG_PACKAGE_EROFS_H_

#include "linglong/util/error.h"
#include "linglong/util/info.h"

namespace linglong::package {

class Erofs : public Compressor
{
public:
    explicit Erofs();

    linglong::utils::error::Result<void> mount(const QString &erofsPath, const QString &mountPoint);

    linglong::utils::error::Result<void> compress(const QString &srcDir,
                                                  const QString &destFilePath) override;

    linglong::utils::error::Result<void> decompress(const QString targetFilePath,
                                                    const QString &destPath,
                                                    const qlonglong &offset) override;

    ~Erofs() override;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_EROFS_H_ */