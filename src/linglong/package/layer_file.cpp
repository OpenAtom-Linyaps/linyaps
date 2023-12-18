/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_file.h"
#include "linglong/package/layer_info.h"

#include <QFileInfo>

namespace linglong::package {

using nlohmann::json;

LayerFile::~LayerFile()
{
    if (this->cleanup) {
        this->remove();
    }
}

void LayerFile::setCleanStatus(bool status)
{
    this->cleanup = status;
}

utils::error::Result<layer::LayerInfo> LayerFile::layerFileInfo()
{
    auto ret = layerInfoSize();
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("failed to get layer file size ", ret.error());
    }

    auto rawData = this->read(qint64(*ret));

    auto layerInfo = fromJson(rawData);
    if (!layerInfo.has_value()) {
        return LINGLONG_EWRAP("failed to get layer info", layerInfo.error());
    }
  
    return layerInfo;
}

utils::error::Result<quint32> LayerFile::layerInfoSize()
{
    if (!this->isOpen() && !this->open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(-1, "failed to open layer file");
    }
    // read from offset 0 everytime
    this->seek(0);

    quint32 layerInfoSize;
    this->read(reinterpret_cast<char *>(&layerInfoSize), sizeof(quint32));

    return layerInfoSize;
}

utils::error::Result<quint32> LayerFile::layerOffset()
{
    auto size = layerInfoSize();
    if (!size.has_value()) {
        return LINGLONG_EWRAP("get LayerInfo size failed", size.error());
    }

    return *size + sizeof(quint32);
}

utils::error::Result<void> LayerFile::saveTo(const QString &destination)
{
    if (!this->copy(destination)) {
        return LINGLONG_ERR(-1, QString("failed to save layer file to %1").arg(destination));
    }
    
    return LINGLONG_OK;
}

} // namespace linglong::package