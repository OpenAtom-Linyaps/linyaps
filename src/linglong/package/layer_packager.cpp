/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/package/layer_info.h"
#include "linglong/util/file.h"
#include "linglong/util/runner.h"

#include <QDataStream>

namespace linglong::package {

LayerPackager::LayerPackager(const QString &workDir)
    : workDir(QStringList{ workDir, QUuid::createUuid().toString(QUuid::Id128) }.join("-"))
{
    util::ensureDir(this->workDir);
}

LayerPackager::~LayerPackager()
{
    util::removeDir(this->workDir);
}

utils::error::Result<QSharedPointer<LayerFile>>
LayerPackager::pack(const LayerDir &dir, const QString &layerFilePath) const
{
    LINGLONG_TRACE("pack layer");

    // compress data with erofs
    const auto compressedFilePath =
      QStringList{ this->workDir, "tmp.erofs" }.join(QDir::separator());

    auto ret = util::Exec("mkfs.erofs",
                          { "-zlz4hc,9", compressedFilePath, dir.absolutePath() },
                          15 * 60 * 1000);
    if (ret) {
        return LINGLONG_ERR("mkfs.erofs: " + ret.message(), ret.code());
    }

    // generate LayerInfo
    layer::LayerInfo layerInfo;
    layerInfo.version = layerInfoVerison;

    auto rawData = dir.rawInfo();
    if (!rawData) {
        return LINGLONG_ERR(rawData);
    }

    // rawData is not checked format
    layerInfo.info = nlohmann::json::parse(*rawData);

    auto layerInfoData = toJson(layerInfo);
    if (!layerInfoData) {
        return LINGLONG_ERR(layerInfoData);
    }
    // write data, [magic number |LayerInfoSize | LayerInfo | compressed data ]
    // open temprary layer file
    QFile layer(layerFilePath);
    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR("open temporary layer file", layer);
    }
    // write magic number, in 40 bytes
    layer.write(magicNumber);

    // write size of LayerInfo, in 4 bytes
    QDataStream out(&layer);
    quint32 layerInfoSize = (*layerInfoData).size();
    out.writeRawData(reinterpret_cast<const char *>(&layerInfoSize), sizeof(quint32));

    // write LayerInfo
    layer.write(*layerInfoData);
    QFile compressedFile(compressedFilePath);
    if (!compressedFile.open(QIODevice::ReadOnly)) {
        return LINGLONG_ERR("open compressed layer file", compressedFile);
    }

    // write compressedFile
    qint64 chunkSize = 2 * 1024 * 1024;
    qint64 remainingSize = compressedFile.size();
    qint64 compressedFileOffset = 0;

    while (remainingSize > 0) {
        qint64 sizeToRead = qMin(chunkSize, remainingSize);
        uchar *compressedData = compressedFile.map(compressedFileOffset, sizeToRead);
        if (!compressedData) {
            return LINGLONG_ERR("mapping compressed file", compressedFile);
        }

        auto ret = layer.write(reinterpret_cast<char *>(compressedData), sizeToRead);
        if (ret < 0) {
            return LINGLONG_ERR("writing data to temporary layer file", layer);
        }

        remainingSize -= sizeToRead;
        compressedFileOffset += sizeToRead;
    }

    // it seems no error here, so i didn't check it
    return LayerFile::openLayer(layerFilePath);
}

utils::error::Result<QSharedPointer<LayerDir>> LayerPackager::unpack(LayerFile &file,
                                                                     const QString &destnation)
{
    LINGLONG_TRACE("unpack layer file");

    auto unpackDir = QStringList{ this->workDir, "unpack" }.join(QDir::separator());
    util::ensureDir(unpackDir);

    QFileInfo fileInfo(file);

    auto offset = file.layerOffset();
    if (!offset) {
        return LINGLONG_ERR(offset);
    }
    auto ret =
      util::Exec("erofsfuse",
                 { QString("--offset=%1").arg(*offset), fileInfo.absoluteFilePath(), unpackDir });
    if (ret) {
        return LINGLONG_ERR("call erofsfuse failed: " + ret.message(), ret.code());
    }

    util::copyDir(unpackDir, destnation);

    ret = util::Exec("umount", { unpackDir });
    if (ret) {
        return LINGLONG_ERR("call umount failed: " + ret.message(), ret.code());
    }

    QSharedPointer<LayerDir> layerDir(new LayerDir(destnation));

    return layerDir;
}

} // namespace linglong::package
