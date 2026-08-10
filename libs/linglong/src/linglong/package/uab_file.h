// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "linglong/api/types/v1/UabMetaInfo.hpp"
#include "linglong/utils/error/error.h"

#include <gelf.h>
#include <libelf.h>

#include <QString>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include <sys/types.h>

namespace linglong::package {

class UABFile
{

    friend class MockUabFile;

public:
    static utils::error::Result<std::unique_ptr<UABFile>>
    loadFromFile(const std::filesystem::path &path) noexcept;
    UABFile(const UABFile &) = delete;
    UABFile &operator=(const UABFile &) = delete;
    UABFile(UABFile &&) = delete;
    UABFile &operator=(UABFile &&) = delete;
    virtual ~UABFile();

    utils::error::Result<bool> verify() noexcept;
    utils::error::Result<void> unpack(const std::filesystem::path &destination) noexcept;

    // Caller should remove destination after use.
    utils::error::Result<std::filesystem::path>
    extractSignData(const std::filesystem::path &destination) noexcept;
    [[nodiscard]] utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
    getMetaInfo() noexcept;

private:
    [[nodiscard]] utils::error::Result<GElf_Shdr>
    getSectionHeader(const QString &section) const noexcept;
    [[nodiscard]] utils::error::Result<std::string> readSectionData(const QString &section,
                                                                    std::size_t offset,
                                                                    std::size_t size) noexcept;
    [[nodiscard]] utils::error::Result<std::reference_wrapper<const api::types::v1::UabMetaInfo>>
    parseMetaInfo(std::string_view content) noexcept;
    UABFile() = default;

    int fd{ -1 };
    Elf *e{ nullptr };
    std::unique_ptr<api::types::v1::UabMetaInfo> metaInfo{ nullptr };
    std::string m_mountPoint;

    // 判断fd是否可在其他进程读取
    virtual bool isFileReadable(const std::string &path) const;
    // 将fd保存为文件，可以避免文件无权限的问题
    virtual utils::error::Result<void> saveErofsToFile(const std::string &path);
    // 创建目录，用于单元测试
    virtual utils::error::Result<void> mkdirDir(const std::string &path) noexcept;
    // 判断命令是否存在
    virtual bool checkCommandExists(const std::string &command) const;
    virtual ssize_t writeData(int fd, const void *data, std::size_t size) const noexcept;
};

} // namespace linglong::package
