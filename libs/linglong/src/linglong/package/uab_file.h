// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/utils/error/error.h"

#include <gelf.h>
#include <libelf.h>

#include <QDir>
#include <QString>

#include <filesystem>

namespace linglong::package {

class UABFile : public QFile
{
public:
    static utils::error::Result<std::shared_ptr<UABFile>> loadFromFile(int fd) noexcept;
    UABFile(UABFile &&) = delete;
    UABFile &operator=(UABFile &&) = delete;
    ~UABFile() override;

    utils::error::Result<bool> verify() noexcept;
    utils::error::Result<std::filesystem::path> mountUab() noexcept;

    // this method will extract sign data to a temporary directory, caller should remove it
    utils::error::Result<std::filesystem::path> extractSignData() noexcept;
    [[nodiscard]] utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
    getMetaInfo() noexcept;

private:
    [[nodiscard]] utils::error::Result<GElf_Shdr>
    getSectionHeader(const QString &section) const noexcept;
    UABFile() = default;

    Elf *e{ nullptr };
    std::unique_ptr<api::types::v1::UabMetaInfo> metaInfo{ nullptr };
    std::filesystem::path mountPoint;
};

} // namespace linglong::package
