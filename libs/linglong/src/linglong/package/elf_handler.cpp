// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "elf_handler.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <LIEF/ELF.hpp>

namespace linglong::package {

utils::error::Result<std::unique_ptr<ElfHandler>> ElfHandler::create(std::filesystem::path file)
{
    LINGLONG_TRACE("create elfhandler: " + QString::fromStdString(file.string()));

    return std::make_unique<ElfHandler>(file);
}

ElfHandler::ElfHandler(std::filesystem::path file)
    : file_(std::move(file))
{
}

ElfHandler::~ElfHandler()
{
}

utils::error::Result<void> ElfHandler::addSection(const std::string &name, const std::filesystem::path &file)
{
    LINGLONG_TRACE("add section from file: " + QString::fromStdString(file.string()));

    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec) {
        return LINGLONG_ERR("failed to get file size for '" + QString::fromStdString(file.string())
                            + "': " + QString::fromStdString(ec.message()));
    }

    if (size == 0) {
        return LINGLONG_ERR("file is empty: " + QString::fromStdString(file.string()));
    }

    int fd = open(file.c_str(), O_RDONLY);
    if (fd < 0) {
        return LINGLONG_ERR("failed to open file: " + QString::fromStdString(file.string()));
    }

    void *data = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);

    if (data == MAP_FAILED) {
        return LINGLONG_ERR("failed to mmap file: " + QString::fromStdString(file.string()));
    }

    auto result = addSection(name, static_cast<const char *>(data), size);

    munmap(data, size);
    close(fd);

    return result;
}

utils::error::Result<void> ElfHandler::addSection(const std::string &name, const char *data, size_t size)
{
    LINGLONG_TRACE("add section:" + QString::fromStdString(name));

    if (name.length() > 16) {
        return LINGLONG_ERR("section name too long");
    }

    std::unique_ptr<LIEF::ELF::Binary> elf = LIEF::ELF::Parser::parse(file_.string());
    if (elf == nullptr) {
        return LINGLONG_ERR("failed to parse elf file: " + QString::fromStdString(file_.string()));
    }

    LIEF::ELF::Section section(name);
    section.content(std::vector<uint8_t>(data, data + size));

    elf->add(section, false);

    elf->write(file_.string());

    return LINGLONG_OK;
}

}
