/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_COMPRESSOR_H_
#define LINGLONG_PACKAGE_COMPRESSOR_H_

#include "linglong/utils/error/error.h"

#include <QString>

namespace linglong::package {

class Compressor
{
public:
    Compressor() = default;
    Compressor(const Compressor &) = delete;
    Compressor(Compressor &&) = delete;
    Compressor &operator=(const Compressor &) = delete;
    Compressor &operator=(Compressor &&) = delete;
    virtual ~Compressor() = default;

    virtual linglong::utils::error::Result<void> compress(const QString &srcDir,
                                                          const QString &destFilePath) = 0;

    virtual linglong::utils::error::Result<void> decompress(const QString targetFilePath,
                                                            const QString &destPath,
                                                            const qlonglong &offset) = 0;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_COMPRESSOR_H_ */
