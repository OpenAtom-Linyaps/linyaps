/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_package.h"

#include "linglong/package/layer_info.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"

#include <QDataStream>

namespace linglong::package {

LayerPackager::LayerPackager(const QString &workDir)
    : workDir(
      QStringList{ workDir, QUuid::createUuid().toString(QUuid::Id128) }.join(QDir::separator()))
{
    util::ensureDir(this->workDir);
}

LayerPackager::~LayerPackager()
{
    util::removeDir(this->workDir);
}

utils::error::Result<QSharedPointer<LayerFile>> LayerPackager::pack(const LayerDir &dir,
                                                                   const QString &destnation) const
{
    // compress data with erofs
    const auto compressedFilePath =
      QStringList{ this->workDir, "tmp.erofs" }.join(QDir::separator());
    const auto layerFilePath = QStringList{ destnation, "tmp.layer" }.join(QDir::separator());
    ;
    auto ret = util::Exec("mkfs.erofs",
                          { "-zlz4hc,9", compressedFilePath, dir.absolutePath() },
                          15 * 60 * 1000);
    if (ret) {
        return LINGLONG_ERR(ret.code(), "call mkfs.erofs failed" + ret.message());
    }

    // generate LayerInfo
    layer::LayerInfo layerInfo;
    layerInfo.version = layerInfoVerison;

    auto rawData = dir.rawInfo();
    if (!rawData.has_value()) {
        return LINGLONG_EWRAP("failed to get raw info data", rawData.error());
    }

    // rawData is not checked format
    layerInfo.info = nlohmann::json::parse(*rawData);

    auto layerInfoData = toJson(layerInfo);
    if (!layerInfoData.has_value()) {
        return LINGLONG_EWRAP("failed to convert LayerInfo to Json data", layerInfoData.error());
    }
    // write data, [LayerInfoSize | LayerInfo | compressed data ]
    // open temprary layer file
    QFile layer(layerFilePath);
    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR(-1, "failed to open temporary file of layer");
    }

    // write size of LayerInfo, in 4 bytes
    QDataStream out(&layer);
    quint32 layerInfoSize = (*layerInfoData).size();
    out.writeRawData(reinterpret_cast<const char *>(&layerInfoSize), sizeof(quint32));

    // write LayerInfo
    layer.write(*layerInfoData);
    QFile compressedFile(compressedFilePath);
    if (!compressedFile.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR(-1, "failed to open compressed data file of layer");
    }

    // write compressedFile
    qint64 chunkSize = 2 * 1024 * 1024;
    qint64 remainingSize = compressedFile.size();
    qint64 compressedFileOffset = 0;

    while (remainingSize > 0) {
        qint64 sizeToRead = qMin(chunkSize, remainingSize);
        uchar *compressedData = compressedFile.map(compressedFileOffset, sizeToRead);
        if (!compressedData) {
            layer.close();
            compressedFile.close();

            return LINGLONG_ERR(-1, "mapping compressed file error");
        }

        auto ret = layer.write(reinterpret_cast<char *>(compressedData), sizeToRead);
        if (ret < 0) {
            compressedFile.unmap(compressedData);
            layer.close();
            compressedFile.close();

            return LINGLONG_ERR(-1, "writing data to temprary layer file failed");
        }

        compressedFile.unmap(compressedData);

        remainingSize -= sizeToRead;
        compressedFileOffset += sizeToRead;
    }

    layer.close();
    compressedFile.close();

    QSharedPointer<LayerFile> layerFile(new LayerFile(layerFilePath));

    return layerFile;
}

utils::error::Result<QSharedPointer<LayerDir>> LayerPackager::unpack(LayerFile &file,
                                                                    const QString &destnation)
{
    auto unpackDir = QStringList{ this->workDir, "unpack" }.join(QDir::separator());
    util::ensureDir(unpackDir);

    QFileInfo fileInfo(file);

    auto offset = file.layerOffset();
    if (!offset.has_value()) {
        return LINGLONG_EWRAP("failed to get layer offset", offset.error());
    }
    auto ret =
      util::Exec("erofsfuse",
                 { QString("--offset=%1").arg(*offset), fileInfo.absoluteFilePath(), unpackDir });
    if (ret) {
        return LINGLONG_ERR(ret.code(), "call erofsfuse failed: " + ret.message());
    }

    util::copyDir(unpackDir, destnation);

    ret = util::Exec("umount", { unpackDir });
    if (ret) {
        return LINGLONG_ERR(ret.code(), "call umount failed: " + ret.message());
    }

    QSharedPointer<LayerDir> layerDir(new LayerDir(destnation));

    return layerDir;
}

} // namespace linglong::package