// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef UAB_PACKAGER_H
#define UAB_PACKAGER_H

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <gelf.h>
#include <libelf.h>

#include <QDir>
#include <QLoggingCategory>
#include <QString>
#include <QUuid>

namespace linglong::package {

Q_DECLARE_LOGGING_CATEGORY(uab_packager)

struct elfHelper
{
    elfHelper() = default;
    elfHelper(const elfHelper &) = delete;
    elfHelper(elfHelper &&) noexcept;
    elfHelper &operator=(const elfHelper &) = delete;
    elfHelper &operator=(elfHelper &&) noexcept;

    friend bool operator==(const elfHelper &lhs, const elfHelper &rhs) noexcept
    {
        return lhs.e == rhs.e && lhs.elfFd == rhs.elfFd && lhs.filePath == rhs.filePath;
    }

    ~elfHelper();
    static utils::error::Result<elfHelper> create(const QByteArray &filePath) noexcept;

    [[nodiscard]] auto parentDir() const { return QFileInfo{ filePath }.absoluteDir(); }

    [[nodiscard]] auto fd() const { return elfFd; }

    [[nodiscard]] auto elfPath() const { return filePath; }

    [[nodiscard]] auto ElfPtr() const { return e; }

    [[nodiscard]] utils::error::Result<void>
    addNewSection(const QByteArray &sectionName, const QFileInfo &dataFile) const noexcept;

private:
    elfHelper(QByteArray path, int fd, Elf *ptr);

    QByteArray filePath;
    int elfFd{ -1 };
    Elf *e{ nullptr };
};

class UABPackager
{
public:
    explicit UABPackager(const QDir &workingDir);
    ~UABPackager();

    UABPackager(UABPackager &&) = delete;

    utils::error::Result<void> setIcon(const QFileInfo &icon);
    utils::error::Result<void> appendLayer(const LayerDir &layer);
    utils::error::Result<void> pack(const QString &uabFilename);

private:
    [[nodiscard]] utils::error::Result<void> packIcon() noexcept;
    [[nodiscard]] utils::error::Result<void> packBundle() noexcept;
    [[nodiscard]] utils::error::Result<void> prepareBundle(const QDir &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void> packMetaInfo() noexcept;

    elfHelper uab;
    QList<LayerDir> layers;
    std::optional<QFileInfo> icon{ std::nullopt };
    api::types::v1::UabMetaInfo meta;
    QDir buildDir;
};
} // namespace linglong::package

#endif
