/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_MODULE_PACKAGE_BUNDLE_H_
#define LINGLONG_BOX_SRC_MODULE_PACKAGE_BUNDLE_H_

#include <elf.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDir>

#include "module/util/result.h"
#include "module/util/file.h"
#include "module/util/status_code.h"
#include "module/util/http/httpclient.h"
#include "info.h"

namespace linglong {
namespace package {

// __LITTLE_ENDIAN or __BIG_ENDIAN
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ELFDATANATIVE ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ELFDATANATIVE ELFDATA2MSB
#else
#error "Unknown machine endian"
#endif

// 16 bit system binary  swap
#define bswap16(value) ((((value)&0xff) << 8) | ((value) >> 8))
// 32 bit system binary  swap
#define bswap32(value) \
    (((uint32_t)bswap16((uint16_t)((value)&0xffff)) << 16) | (uint32_t)bswap16((uint16_t)((value) >> 16)))
// 64 bit system binary  swap
#define bswap64(value) \
    (((uint64_t)bswap32((uint32_t)((value)&0xffffffff)) << 32) | (uint64_t)bswap32((uint32_t)((value) >> 32)))

// FIXME: there is some problem that in module/util/runner.h, replace later
linglong::util::Error runner(const QString &program, const QStringList &args, int timeout = -1);

class BundlePrivate;

/*
 * Bundle
 * Create Bundle format file, An Bundle contains loader, and it's squashfs.
 *
 */
class Bundle : public QObject
{
    Q_OBJECT

public:
    explicit Bundle(QObject *parent = nullptr);
    ~Bundle();

    /**
     * Load Bundle from path, create parent if not exist
     * @param path
     * @return
     */
    linglong::util::Error load(const QString &path);

    /**
     * Save Bundle to path, create parent if not exist
     * @param path
     * @return
     */
    linglong::util::Error save(const QString &path);

    // Info info() const;

    /**
     * make Bundle
     * @param dataPath : data path
     * @param outputFilePath : output file path
     * @return Result
     */
    linglong::util::Error make(const QString &dataPath, const QString &outputFilePath);

    /**
     * push Bundle
     * @param uabFilePath : uab file path
     * @param repoUrl : remote repo url
     * @param force :  force to push
     * @return Result
     */
    linglong::util::Error push(const QString &bundleFilePath, const QString &repoUrl, const QString &repoChannel, bool force);

private:
    QScopedPointer<BundlePrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Bundle)
};

} // namespace package
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_MODULE_PACKAGE_BUNDLE_H_ */
