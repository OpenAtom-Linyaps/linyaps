/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_H_
#define LINGLONG_PACKAGE_LAYER_H_

#include "linglong/util/error.h"
#include "linglong/util/info.h"
#include "linglong/util/qserializer/deprecated.h"

#include <QString>

namespace linglong::package {

// layer [LayerInfoSize|LayerInfo|data]
class LayerDir;
class LayerInfo;

class Layer
{
public:
    explicit Layer(const LayerDir &dir);
    ~Layer();

    linglong::utils::error::Result<void> makeLayer(const QString &layerPath);

    linglong::utils::error::Result<LayerInfo> getLayerInfo(const QString &layerPath);

    linglong::utils::error::Result<QString> exportLayerData(const QString &layerPath,
                                                            const QString &destPath);

private:
    linglong::utils::error::Result<void> generateLayerInfo();

    linglong::utils::error::Result<void> compressData();
    size_t layerInfoOffset;
    QString layerWorkDir;
    LayerDir layerDir;
};

class LayerInfo : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(LayerInfo)
public:
    Q_JSON_PROPERTY(QString, version);
    Q_JSON_PROPERTY(qlonglong, size);

    Q_JSON_PTR_PROPERTY(linglong::package::Info, pkgInfo);
};

class LayerDir
{
public:
    explicit LayerDir(const QString &targetPath);

    const QString getInfoPath() { return infoPath; }

    const QString getDataPath() { return dataPath; }

private:
    QString infoPath;
    QString dataPath;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_H_ */