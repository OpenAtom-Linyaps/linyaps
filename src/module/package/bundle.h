/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QDir>

#include "module/util/result.h"
#include "module/util/fs.h"
#include "module/package/package.h"
#include "service/impl/dbus_retcode.h"

namespace linglong {
namespace package {

util::Result runner(const QString &program, const QStringList &args, int timeout = -1);

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
    util::Result load(const QString &path);

    /**
     * Save Bundle to path, create parent if not exist
     * @param path
     * @return
     */
    util::Result save(const QString &path);

    // Info info() const;

    /**
     * make Bundle
     * @param dataPath : data path
     * @param outputFilePath : output file path
     * @return Result
     */
    util::Result make(const QString &dataPath, const QString &outputFilePath);


private:
    QScopedPointer<BundlePrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), Bundle)
};

} // namespace package
} // namespace linglong