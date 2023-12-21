/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_PACKAGE_LAYER_FILE_H_
#define LINGLONG_PACKAGE_LAYER_FILE_H_

#include "linglong/package/layer/LayerInfo.hpp"
#include "linglong/utils/error/error.h"

#include <QFile>

namespace linglong::package {

// layer file [magic number 40bytes | LayerInfoSize 4bytes | LayerInfo | compressedData]
// fill magicNumber with 0 to 40bytes
const QByteArray magicNumber = QByteArray("<<< deepin linglong layer archive >>>").leftJustified(40, 0);

class LayerFile : public QFile
{
public:
    ~LayerFile();
    utils::error::Result<layer::LayerInfo> layerFileInfo() noexcept;

    utils::error::Result<quint32> layerOffset() noexcept;

    utils::error::Result<void> saveTo(const QString &destination) noexcept;

    void setCleanStatus(bool status) noexcept;

    static utils::error::Result<QSharedPointer<LayerFile>> openLayer(const QString &path) noexcept;

private:
    explicit LayerFile(const QString &path);
    utils::error::Result<quint32> layerInfoSize();

    bool cleanup = false;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_FILE_H_ */
