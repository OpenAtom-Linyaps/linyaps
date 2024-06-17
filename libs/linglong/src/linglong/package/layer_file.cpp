/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_file.h"

#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/serialize/json.h"

#include <QDataStream>
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

utils::error::Result<QSharedPointer<LayerFile>> LayerFile::New(const QString &path) noexcept

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

utils::error::Result<api::types::v1::LayerInfo> LayerFile::metaInfo() noexcept
{
    LINGLONG_TRACE("get layer file info");

    auto ret = this->metaInfoLength();
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    auto rawData = this->read(qint64(*ret));

    auto layerInfo = utils::serialize::LoadJSON<api::types::v1::LayerInfo>(rawData);
    if (!layerInfo) {
        return LINGLONG_ERR(layerInfo);
    }

    return layerInfo;
}

utils::error::Result<quint32> LayerFile::metaInfoLength()
{
    LINGLONG_TRACE("read meta info length");

    if (metaInfoLengthValue > 0) {
        return metaInfoLengthValue;
    }

    QDataStream layerDataStream(this);

    layerDataStream.startTransaction();
    layerDataStream.setByteOrder(QDataStream::LittleEndian);
    layerDataStream >> metaInfoLengthValue;

    if (!layerDataStream.commitTransaction()) {
        return LINGLONG_ERR("unknown error.");
    }

    return metaInfoLengthValue;
}

utils::error::Result<quint32> LayerFile::binaryDataOffset() noexcept
{
    LINGLONG_TRACE("get binary data offset");

    auto size = this->metaInfoLength();
    if (!size) {
        return LINGLONG_ERR(size);
    }

    return magicNumber.size() + *size + sizeof(quint32);
}

utils::error::Result<void> LayerFile::saveTo(const QString &destination) noexcept
{
    LINGLONG_TRACE(QString("save layer file to %1").arg(destination));

    if (!this->copy(destination)) {
        return LINGLONG_ERR(*this);
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
