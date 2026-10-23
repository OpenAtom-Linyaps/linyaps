/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/package/layer_file.h"

#include "linglong/api/types/v1/Generators.hpp" // IWYU pragma: keep
#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/common/error.h"
#include "linglong/common/formatter.h"
#include "linglong/utils/error/error.h"
#include "linglong/utils/log/formatter.h" // IWYU pragma: keep
#include "linglong/utils/serialize/json.h"

#include <QDataStream>
#include <QFileInfo>

#include <fcntl.h>

namespace linglong::package {

using nlohmann::json;

LayerFile::~LayerFile()
{
    if (this->cleanup) {
        this->remove();
    }
}

utils::error::Result<QSharedPointer<LayerFile>> LayerFile::New(const QString &path) noexcept
{
    LINGLONG_TRACE("install layer file from path")
    auto fd = ::open(path.toLocal8Bit(), O_RDONLY);
    if (fd < 0) {
        return LINGLONG_ERR(
          fmt::format("failed to open {}: {}", path, common::error::errorString(errno)));
    }

    return New(fd);
}

utils::error::Result<QSharedPointer<LayerFile>> LayerFile::New(int fd) noexcept
{
    LINGLONG_TRACE("install layer file from file descriptor")

    struct helper : public LayerFile
    {
    };

    auto file = QSharedPointer<helper>::create();
    if (!file) {
        return LINGLONG_ERR("failed to create LayerFile object");
    }

    if (!file->open(fd, QIODevice::ReadOnly, FileHandleFlag::AutoCloseHandle)) {
        return LINGLONG_ERR("open layer failed");
    }

    const auto &number = magicNumber();
    if (file->read(number.size()) != number) {
        return LINGLONG_ERR("invalid magic number, this is not a layer");
    }

    return file;
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
    if (rawData.size() != qint64(*ret)) {
        return LINGLONG_ERR("truncated layer metadata");
    }
    if (rawData.contains('\0')) {
        return LINGLONG_ERR("layer metadata contains a NUL byte");
    }

    auto layerInfo = utils::serialize::LoadJSON<api::types::v1::LayerInfo>(rawData.toStdString());
    if (!layerInfo) {
        return LINGLONG_ERR(layerInfo);
    }

    return layerInfo;
}

utils::error::Result<quint32> LayerFile::metaInfoLength()
{
    LINGLONG_TRACE("read meta info length");

    const qint64 headerSize = magicNumber().size() + sizeof(quint32);
    const auto fileSize = this->size();
    if (fileSize < headerSize) {
        return LINGLONG_ERR("truncated layer header");
    }
    // Always read from the header, independently of previous metadata/offset queries.
    if (!this->seek(magicNumber().size())) {
        return LINGLONG_ERR("failed to seek to layer metadata length");
    }

    QDataStream layerDataStream(this);
    layerDataStream.setByteOrder(QDataStream::LittleEndian);
    quint32 length = 0;
    layerDataStream >> length;
    if (layerDataStream.status() != QDataStream::Ok) {
        return LINGLONG_ERR("failed to read layer metadata length");
    }
    if (length == 0 || length > maxMetadataSize) {
        return LINGLONG_ERR("layer metadata length is outside the supported range (1..16 MiB)");
    }
    if (qint64(length) > fileSize - headerSize) {
        return LINGLONG_ERR("layer metadata extends beyond the file size");
    }

    return length;
}

utils::error::Result<quint64> LayerFile::binaryDataOffset() noexcept
{
    LINGLONG_TRACE("get binary data offset");

    auto size = this->metaInfoLength();
    if (!size) {
        return LINGLONG_ERR(size);
    }

    return quint64(magicNumber().size()) + *size + sizeof(quint32);
}

utils::error::Result<void> LayerFile::saveTo(const QString &destination) noexcept
{
    LINGLONG_TRACE(fmt::format("save layer file to {}", destination.toStdString()));

    if (!this->copy(destination)) {
        return LINGLONG_ERR(this->errorString().toStdString());
    }

    return LINGLONG_OK;
}

} // namespace linglong::package
