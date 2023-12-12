/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_H_
#define LINGLONG_PACKAGE_LAYER_H_

#include "linglong/package/info.h"
#include "linglong/util/qserializer/deprecated.h"
#include "linglong/utils/error/error.h"

#include <QDir>
#include <QString>

#include <optional>

namespace linglong::package {

class LayerInfo : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(LayerInfo)
public:
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(qlonglong, size);

    Q_JSON_PTR_PROPERTY(linglong::package::Info, pkgInfo);
};

class LayerFile : public QFile
{
public:
    using QFile::QFile;
    utils::error::Result<LayerInfo> layerFileInfo() const;
    QFile release();
private:
    bool cleanup = true;
};

class LayerDir : public QDir
{
public:
    using QDir::QDir;
    utils::error::Result<Info> info() const;
    QDir release();
};

class LayerPackager : public QObject
{
public:
    utils::error::Result<LayerFile> pack(const LayerDir &dir, const QString &destination) const;
    utils::error::Result<LayerDir> unpack(const LayerFile &file, const QString &destination) const;
};

class ErofsLayerPackager : public LayerPackager
{
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_H_ */
