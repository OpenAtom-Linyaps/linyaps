/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer.h"

#include "linglong/package/erofs.h"
#include "linglong/util/runner.h"

namespace linglong::package {

const char *const layerInfoVerison = "0.1";

Layer::Layer(const LayerDir &dir)
    : layerDir(dir)
    , layerInfoOffset(sizeof(int))
    , layerWorkDir("/tmp/linglong-layer")
{
}

linglong::utils::error::Result<void> Layer::makeLayer(const QString &layerPath)
{
    compressData();
    generateLayerInfo();
    // write data, [LayerInfoSize | LayerInfo | compressed data ]
    QFile layer(layerPath);
    if (!layer.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(-1, "failed to open temporary file of layer");
    }

    // write size of LayerInfo
    QFile layerInfoFile{};
    if (!layerInfoFile.open(QIODevice::WriteOnly)) {
        return LINGLONG_ERR(-1, "failed to open the json of layerInfo");
    }
    layer.write(layerInfoFile.size());
    // write LayerInfo
    layer.seek(layerInfoOffset);
    layer.write(layerInfoFile.readAll());

    QFile compressedFile(tmpDataPath);
    if (!compressedFile.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(-1, "failed to open compressed data file of layer");
    }

    // write compressedFile
    qint64 chunkSize = 2 * 1024 * 1024;
    qint64 layerOffset = layerInfoOffset + layerInfoFile.size();
    qint64 remainingSize = compressedFile.size();
    qint64 compressedFileOffset = 0;

    while (remainingSize > 0) {
        qint64 sizeToRead = qMin(chunkSize, remainingSize);
        uchar *compressedData = compressedFile.map(compressedFileOffset, sizeToRead);
        uchar *layerData = layer.map(layerOffset, sizeToRead);

        std::memcpy(layerData, compressedData, sizeToRead);

        compressedFile.unmap(compressedData);
        targetFile.unmap(layerData);

        remainingSize -= sizeToRead;
        layerOffset += sizeToRead;
        compressedFileOffset += sizeToRead;
    }

    layer.close();
    compressedFile.close();
}

linglong::utils::error::Result<LayerInfo> Layer::readLayerInfo(const QString &layerPath)
{
    QFile layer(layerPath);
    if (!layer.open(QIODevice::READONLY)) {
        return LINGLONG_ERR(-1, "failed to open layer");
    }

    auto size = layer.read(layerInfoOffset);
    auto data = layer.read();

    LayerInfo layerInfo = layer.read(xx);
    return layerInfo;
}

linglong::utils::error::Result<void> Layer::exportLayerData(const QString &layerPath,
                                                            const QString &destPath)
{
    auto layerInfo = readLayerInfo(layerPath);

    qlonglong offset = layerInfoOffset + layerInfo->size();
    QString layerDataPath;
    Compressor *c = new Erofs();

    auto ret = c.decompress(layerPath, destPath, offset);
    if (!ret.has_value()) {
        LINGLONG_EWRAP("failed to decompress the data file of layer", ret.error())
    }

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> Layer::generateLayerInfo()
{
    auto [info, err] = util::fromJSON<QSharedPointer<Info>>(layerDir.getInfoPath());
    if (err) {
        return LINGLONG_ERR(err.code(), "failed to parse info.json");
    }

    QFile compressedFile(tmpCompressedFile);

    LayerInfo layerInfo;
    layerInfo.pkgInfo = info;
    layerInfo.version = layerInfoVerison;
    layerInfo.size = compressedFile.size();

    auto layerInfoData = std::get<0>(util::toJSON(layerInfo);
    // write layerInfo

    return LINGLONG_OK;
}

linglong::utils::error::Result<void> Layer::compressData()
{
    // compress the data part of layer
    Compressor *c = new Erofs();
    auto ret = c.compress(layerDir.getDataPath(), tmpDataPath);
    if (!ret.has_value()) {
        LINGLONG_EWRAP("failed to compress the data file of layer", ret.error());
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
