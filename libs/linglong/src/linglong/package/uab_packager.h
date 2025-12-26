// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/package/elf_handler.h"
#include "linglong/package/layer_dir.h"
#include "linglong/utils/error/error.h"

#include <gelf.h>
#include <libelf.h>

#include <QDir>
#include <QString>
#include <QUuid>

#include <filesystem>
#include <memory>
#include <unordered_set>

namespace linglong::package {

class UABPackager
{
public:
    explicit UABPackager(const QDir &projectDir, QDir workingDir);
    ~UABPackager();

    UABPackager(UABPackager &&) = delete;

    utils::error::Result<void> setIcon(std::filesystem::path icon) noexcept;
    utils::error::Result<void> appendLayer(const LayerDir &layer) noexcept;
    utils::error::Result<void> pack(const QString &uabFilePath, bool distributedOnly) noexcept;
    utils::error::Result<void> exclude(const std::vector<std::string> &files) noexcept;
    utils::error::Result<void> include(const std::vector<std::string> &files) noexcept;
    utils::error::Result<void> loadBlackList() noexcept;
    utils::error::Result<void> loadNeededFiles() noexcept;
    void setLoader(const QString &loader) noexcept;
    void setCompressor(const QString &compressor) noexcept;
    void setDefaultHeader(const QString &header) noexcept;
    void setDefaultLoader(const QString &loader) noexcept;
    void setDefaultBox(const QString &box) noexcept;
    void setBundleCB(std::function<utils::error::Result<void>(const QString &, const QString &)>
                       bundleCB) noexcept;

private:
    [[nodiscard]] utils::error::Result<void> packIcon() noexcept;
    [[nodiscard]] utils::error::Result<void> packBundle(bool distributed) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareExecutableBundle(const QDir &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void>
    prepareDistributedBundle(const QDir &bundleDir) noexcept;
    [[nodiscard]] utils::error::Result<void> packMetaInfo() noexcept;
    [[nodiscard]] utils::error::Result<std::pair<bool, std::unordered_set<std::string>>>
    filteringFiles(const LayerDir &layer) const noexcept;

    std::unique_ptr<ElfHandler> uab;
    QList<LayerDir> layers;
    std::unordered_set<std::string> excludeFiles;
    std::unordered_set<std::string> includeFiles;
    std::unordered_set<std::string> neededFiles;
    std::unordered_set<std::string> blackList;
    std::optional<std::filesystem::path> icon;
    api::types::v1::UabMetaInfo meta;
    QDir buildDir;
    std::filesystem::path workDir;
    QString loader;
    QString compressor = "lz4";
    QString defaultHeader;
    QString defaultLoader;
    QString defaultBox;
    std::function<utils::error::Result<void>(const QString &, const QString &)> bundleCB;
};
} // namespace linglong::package
