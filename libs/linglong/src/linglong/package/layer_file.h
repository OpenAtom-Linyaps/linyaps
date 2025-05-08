/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#pragma once

#include "linglong/api/types/v1/LayerInfo.hpp"
#include "linglong/utils/error/error.h"

#include <QFile>

namespace linglong::package {

const QByteArray magicNumber =
  QByteArray("<<< deepin linglong layer archive >>>").leftJustified(40, 0);

// LayerFile format:
//
// Name              Length (bytes)    Starts at (bytes)
// magic number      40                0
// meta info length  4                 40
// meta info         meta info length  44
// binary data                         44 + meta info length
class LayerFile : public QFile
{
public:
    LayerFile(const LayerFile &) = delete;
    LayerFile(LayerFile &&) = delete;
    LayerFile &operator=(const LayerFile &) = delete;
    LayerFile &operator=(LayerFile &&) = delete;
    ~LayerFile() override;

    utils::error::Result<api::types::v1::LayerInfo> metaInfo() noexcept;

    utils::error::Result<quint32> binaryDataOffset() noexcept;

    utils::error::Result<void> saveTo(const QString &destination) noexcept;

    // NOTE: Maybe should be removed. and use QTemporaryFile
    void setCleanStatus(bool status) noexcept;

    static utils::error::Result<QSharedPointer<LayerFile>> New(int fd) noexcept;
    static utils::error::Result<QSharedPointer<LayerFile>> New(const QString &path) noexcept;

private:
    LayerFile() = default;
    utils::error::Result<quint32> metaInfoLength();

    bool cleanup = false;
    quint32 metaInfoLengthValue = 0;
};

} // namespace linglong::package
