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
#include <string>

namespace linglong::package {

class UABFile : public QFile
{

    friend class MockUabFile;

public:
    static utils::error::Result<std::shared_ptr<UABFile>> loadFromFile(int fd) noexcept;
    UABFile(UABFile &&) = delete;
    UABFile &operator=(UABFile &&) = delete;
    ~UABFile() override;

    utils::error::Result<bool> verify() noexcept;
    utils::error::Result<std::filesystem::path> unpack() noexcept;

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
    std::string m_mountPoint;
    std::string m_unpackPath;

    // 判断fd是否可在其他进程读取
    virtual bool isFileReadable(const std::string &path) const;
    // 将fd保存为文件，可以避免文件无权限的问题
    virtual utils::error::Result<void> saveErofsToFile(const std::string &path);
    // 创建目录，用于单元测试
    virtual utils::error::Result<void> mkdirDir(const std::string &path) noexcept;
    // 判断命令是否存在
    virtual bool checkCommandExists(const std::string &command) const;
};

} // namespace linglong::package
