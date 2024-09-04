// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <gelf.h>
#include <libelf.h>

#include <QDir>
#include <QLoggingCategory>
#include <QString>
#include <QUuid>

#include <filesystem>
#include <unordered_set>

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

    utils::error::Result<void> setIcon(const QFileInfo &icon) noexcept;
    utils::error::Result<void> appendLayer(const LayerDir &layer) noexcept;
    utils::error::Result<void> pack(const QString &uabFilename) noexcept;
    utils::error::Result<void> exclude(const std::vector<std::string> &files) noexcept;
    utils::error::Result<void> include(const std::vector<std::string> &files) noexcept;

private:
    [[nodiscard]] utils::error::Result<void> packIcon() noexcept;
    [[nodiscard]] utils::error::Result<void> packBundle() noexcept;
    [[nodiscard]] utils::error::Result<void> prepareBundle(const QDir &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void> packMetaInfo() noexcept;
    [[nodiscard]] utils::error::Result<std::pair<bool, std::unordered_set<std::string>>>
    filteringFiles(const LayerDir &layer) const noexcept;

    elfHelper uab;
    QList<LayerDir> layers;
    std::unordered_set<std::string> excludeFiles;
    std::unordered_set<std::string> includeFiles;
    std::optional<QFileInfo> icon{ std::nullopt };
    api::types::v1::UabMetaInfo meta;
    QDir buildDir;
};
} // namespace linglong::package
