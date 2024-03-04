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

try {
    QSharedPointer<LayerFile> layerFile(new LayerFile(path));
    return layerFile;
} catch (const std::exception &e) {
    LINGLONG_TRACE("open layer");
    return LINGLONG_ERR(e);
}

void LayerFile::setCleanStatus(bool status) noexcept
{
    this->cleanup = status;
}

utils::error::Result<layer::LayerInfo> LayerFile::layerFileInfo() noexcept
{
    LINGLONG_TRACE("get layer file info");

    auto ret = layerInfoSize();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    auto rawData = this->read(qint64(*ret));

    auto layerInfo = fromJson(rawData);
    if (!layerInfo) {
        return LINGLONG_ERR(layerInfo);
    }

    return layerInfo;
}

utils::error::Result<quint32> LayerFile::layerInfoSize()
{
    LINGLONG_TRACE("get layer file info");

    // read from position magicNumber.size() everytime
    this->seek(magicNumber.size());

    quint32 layerInfoSize = 0;
    this->read(reinterpret_cast<char *>(&layerInfoSize), sizeof(quint32));

    return layerInfoSize;
}

utils::error::Result<quint32> LayerFile::layerOffset() noexcept
{
    LINGLONG_TRACE("get layer offset");

    auto size = layerInfoSize();
    if (!size) {
        return LINGLONG_ERR(size);
    }

    return magicNumber.size() + *size + sizeof(quint32);
}

utils::error::Result<void> LayerFile::saveTo(const QString &destination) noexcept
{
    LINGLONG_TRACE(QString("save layer file to %1").arg(destination));
    if (!this->copy(destination)) {
        return LINGLONG_ERR("copy failed: " + this->errorString(), this->error());
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
