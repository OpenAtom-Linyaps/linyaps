// SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "linglong/package/uab_file.h"
#include "linglong/utils/error/error.h"

#include <functional>
#include <string>

#include <fcntl.h>

using namespace linglong;

namespace linglong::package {

class MockUabFile : public package::UABFile
{
public:
    // Mock virtual methods that need to be overridden for testing
    std::function<bool(const std::string &)> wrapIsFileReadableFunc;
    std::function<utils::error::Result<void>(const std::string &)> wrapSaveErofsToFileFunc;
    std::function<utils::error::Result<void>(const std::string &)> wrapMkdirDirFunc;
    std::function<bool(const std::string &)> wrapCheckCommandExistsFunc;
    std::function<ssize_t(int, const void *, std::size_t)> wrapWriteDataFunc;

    explicit MockUabFile(const std::string &path)
    {
        this->UABFile::fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        elf_version(EV_CURRENT);
        auto *elf = elf_begin(this->UABFile::fd, ELF_C_READ, nullptr);
        this->UABFile::e = elf;
    }

protected:
    bool isFileReadable(const std::string &path) const override
    {
        return wrapIsFileReadableFunc ? wrapIsFileReadableFunc(path)
                                      : UABFile::isFileReadable(path);
    }

    utils::error::Result<void> saveErofsToFile(const std::string &path) override
    {
        return wrapSaveErofsToFileFunc ? wrapSaveErofsToFileFunc(path)
                                       : UABFile::saveErofsToFile(path);
    }

    utils::error::Result<void> mkdirDir(const std::string &path) noexcept override
    {
        return wrapMkdirDirFunc ? wrapMkdirDirFunc(path) : UABFile::mkdirDir(path);
    }

    bool checkCommandExists(const std::string &command) const override
    {
        return wrapCheckCommandExistsFunc ? wrapCheckCommandExistsFunc(command)
                                          : UABFile::checkCommandExists(command);
    }

    ssize_t writeData(int fd, const void *data, std::size_t size) const noexcept override
    {
        return wrapWriteDataFunc ? wrapWriteDataFunc(fd, data, size)
                                 : UABFile::writeData(fd, data, size);
    }
};

} // namespace linglong::package
