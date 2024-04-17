/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_packager.h"

#include "linglong/api/types/v1/Generators.hpp"
#include "linglong/utils/command/env.h"

#include <QDataStream>
#include <QSysInfo>

namespace linglong::package {

LayerPackager::LayerPackager(const QDir &workDir)
    : workDir(workDir.absoluteFilePath(QUuid::createUuid().toString(QUuid::Id128)))
{
    if (this->workDir.mkpath(".")) {
        return;
    }

    qCritical() << "mkpath" << this->workDir << "failed";
    Q_ASSERT(false);
}

LayerPackager::~LayerPackager()
{
    for (const auto &info : this->workDir.entryInfoList()) {
        if (!info.isDir()) {
            continue;
        }

        auto ret = utils::command::Exec("umount", { info.absolutePath() });
        if (!ret) {
            qCritical() << ret.error();
            Q_ASSERT(false);
        }
    }

    if (this->workDir.removeRecursively()) {
        return;
    }

    qCritical() << "remove" << this->workDir << "failed";
    Q_ASSERT(false);
}

utils::error::Result<QSharedPointer<LayerFile>>
LayerPackager::pack(const LayerDir &dir, const QString &layerFilePath) const
{
    LINGLONG_TRACE("pack layer");

    QFile layer(layerFilePath);
    if (!layer.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(magicNumber) < 0) {
        return LINGLONG_ERR(layer);
    }

    auto info = dir.info();
    if (!info) {
        return LINGLONG_ERR(info);
    }

    auto json = nlohmann::json(*info);
    auto data = QByteArray::fromStdString(json.dump());

    QByteArray dataSizeBytes;

    QDataStream dataSizeStream(&dataSizeBytes, QIODevice::WriteOnly);
    dataSizeStream.setVersion(QDataStream::Qt_5_10);
    dataSizeStream.setByteOrder(QDataStream::LittleEndian);
    dataSizeStream << quint32(data.size());

    Q_ASSERT(dataSizeStream.status() == QDataStream::Status::Ok);

    if (layer.write(dataSizeBytes) < 0) {
        return LINGLONG_ERR(layer);
    }

    if (layer.write(data) < 0) {
        return LINGLONG_ERR(layer);
    }

    layer.close();

    // compress data with erofs
    const auto compressedFilePath = this->workDir.absoluteFilePath("tmp.erofs");

    auto ret =
      utils::command::Exec("mkfs.erofs", { "-zlz4hc,9", compressedFilePath, dir.absolutePath() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    ret = utils::command::Exec(
      "sh",
      { "-c", QString("cat %1 >> %2").arg(compressedFilePath, layerFilePath) });
    if (!ret) {
        LINGLONG_ERR(ret);
    }

    auto result = LayerFile::New(layerFilePath);
    Q_ASSERT(result.has_value());

    return result;
}

utils::error::Result<LayerDir> LayerPackager::unpack(LayerFile &file)
{
    LINGLONG_TRACE("unpack layer file");

    auto unpackDir = QDir(this->workDir.absoluteFilePath("unpack"));
    unpackDir.mkpath(".");

    QFileInfo fileInfo(file);

    auto offset = file.binaryDataOffset();
    if (!offset) {
        return LINGLONG_ERR(offset);
    }

    auto ret = utils::command::Exec("erofsfuse",
                                    { QString("--offset=%1").arg(*offset),
                                      fileInfo.absoluteFilePath(),
                                      unpackDir.absolutePath() });
    if (!ret) {
        return LINGLONG_ERR(ret);
    }

    return unpackDir.absolutePath();
}

} // namespace linglong::package
