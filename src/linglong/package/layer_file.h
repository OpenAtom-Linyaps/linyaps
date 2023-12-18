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

// layer file [LayerInfoSize | LayerInfo | compressedData]
class LayerFile : public QFile
{
public:
    using QFile::QFile;
    ~LayerFile();
    utils::error::Result<layer::LayerInfo> layerFileInfo();

    utils::error::Result<quint32> layerOffset();

    utils::error::Result<void> saveTo(const QString &destination);

    void setCleanStatus(bool status);

private:
    utils::error::Result<quint32> layerInfoSize();

    bool cleanup = false;
};

} // namespace linglong::package

#endif /* LINGLONG_PACKAGE_LAYER_FILE_H_ */
