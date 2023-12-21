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

LayerFile::LayerFile(const QString &path)
    : QFile(path)
{
    if (!this->open(QIODevice::ReadOnly)) {
        throw std::runtime_error("open layer failed");
    }

    if (this->read(magicNumber.size()) != magicNumber) {
        throw std::runtime_error("invalid magic number, this is not a layer");
    }
}

LayerFile::~LayerFile()
{
    if (this->cleanup) {
        this->remove();
    }
}

utils::error::Result<QSharedPointer<LayerFile>> LayerFile::openLayer(const QString &path) noexcept
{
    try {
        QSharedPointer<LayerFile> layerFile(new LayerFile(path));
        return layerFile;
    } catch (const std::exception &e) {
        return LINGLONG_ERR(-1, e.what());
    }
}

void LayerFile::setCleanStatus(bool status) noexcept
{
    this->cleanup = status;
}

utils::error::Result<layer::LayerInfo> LayerFile::layerFileInfo() noexcept
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
    // read from position magicNumber.size() everytime
    this->seek(magicNumber.size());

    quint32 layerInfoSize;
    this->read(reinterpret_cast<char *>(&layerInfoSize), sizeof(quint32));

    return layerInfoSize;
}

utils::error::Result<quint32> LayerFile::layerOffset() noexcept
{
    auto size = layerInfoSize();
    if (!size.has_value()) {
        return LINGLONG_EWRAP("get LayerInfo size failed", size.error());
    }

    return magicNumber.size() + *size + sizeof(quint32);
}

utils::error::Result<void> LayerFile::saveTo(const QString &destination) noexcept
{
    if (!this->copy(destination)) {
        return LINGLONG_ERR(-1, QString("failed to save layer file to %1").arg(destination));
    }
    
    return LINGLONG_OK;
}

} // namespace linglong::package