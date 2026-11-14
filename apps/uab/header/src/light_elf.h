// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "utils.h"

#include <elf.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lightElf {

constexpr std::size_t machine_type = sizeof(void *) == 8 ? 64 : 32;
template <std::size_t Bit>
struct Elf_trait;

template <>
struct Elf_trait<64> // NOLINT
{
    using elf_header = Elf64_Ehdr;
    using program_header = Elf64_Phdr;
    using section_header = Elf64_Shdr;
};

template <>
struct Elf_trait<32> // NOLINT
{
    using elf_header = Elf32_Ehdr;
    using program_header = Elf32_Phdr;
    using section_header = Elf32_Shdr;
};

template <std::size_t Bit>
class Elf
{
public:
    using SectionHeader = typename Elf_trait<Bit>::section_header;
    using ElfHeader = typename Elf_trait<Bit>::elf_header;
    using ProgramHeader = typename Elf_trait<Bit>::program_header;

    Elf() = delete;
    Elf(const Elf &) = delete;
    Elf &operator=(const Elf &) = delete;
    Elf(Elf &&) = delete;
    Elf &operator=(Elf &&) = delete;

    ~Elf()
    {
        if (fd != -1) {
            ::close(fd);
        }
    }

    explicit Elf(const std::filesystem::path &path)
    {
        auto fd = ::open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            throw std::runtime_error("failed to open " + path.string() + ": " + ::strerror(errno));
        }

        auto closeIfError = defer([&fd]() {
            if (fd != -1) {
                ::close(fd);
            }
        });

        ElfHeader elfHeader;
        auto bytesRead = ::pread(fd, &elfHeader, section_size, 0);
        if (bytesRead == -1) {
            throw std::runtime_error("failed to read " + path.string() + ": " + ::strerror(errno));
        }
        if (bytesRead != section_size) {
            throw std::runtime_error("incomplete read header from " + path.string());
        }

        if (elfHeader.e_ident[EI_MAG0] != ELFMAG0 || elfHeader.e_ident[EI_MAG1] != ELFMAG1
            || elfHeader.e_ident[EI_MAG2] != ELFMAG2 || elfHeader.e_ident[EI_MAG3] != ELFMAG3) {
            throw std::runtime_error(path.string() + "is not an elf file");
        }

        /*
         * e_shentsize is attacker-controlled and is used to advance through
         * the section header table.  Only the native section header size is
         * valid: a smaller value makes entries overlap, and a larger one lets
         * callers read past a SectionHeader buffer.
         */
        if (elfHeader.e_shentsize != section_size) {
            throw std::runtime_error("unexpected section header entry size "
                                     + std::to_string(elfHeader.e_shentsize) + ", expected "
                                     + std::to_string(section_size));
        }

        if (elfHeader.e_shoff == 0) {
            throw std::runtime_error("current elf does not has section header table");
        }

        auto shdrstrndx = elfHeader.e_shstrndx;
        if (shdrstrndx == SHN_UNDEF) {
            throw std::runtime_error("current elf does not has section header string table");
        }

        SectionHeader shdr;
        if (shdrstrndx == SHN_XINDEX) {
            bytesRead = ::pread(fd, &shdr, section_size, elfHeader.e_shoff);
            if (bytesRead == -1) {
                throw std::runtime_error("failed to read initial section header of" + path.string()
                                         + ": " + ::strerror(errno));
            }
            if (bytesRead != section_size) {
                throw std::runtime_error("incomplete read initial section header of"
                                         + path.string());
            }

            if (shdr.sh_link == 0) {
                throw std::runtime_error("current elf is invalid, sh_link of initial "
                                         "section header is 0 but e_shstrdx is"
                                         + std::to_string(SHN_XINDEX));
            }

            shdrstrndx = shdr.sh_link;
        }

        auto shdrstrtab = elfHeader.e_shoff + (shdrstrndx * elfHeader.e_shentsize);
        bytesRead = ::pread(fd, &shdr, section_size, shdrstrtab);
        if (bytesRead == -1) {
            throw std::runtime_error("failed to read section header string table of" + path.string()
                                     + ": " + ::strerror(errno));
        }
        if (bytesRead != section_size) {
            throw std::runtime_error("incomplete read section header string table of"
                                     + path.string());
        }

        if (shdr.sh_type != SHT_STRTAB) {
            throw std::runtime_error("the type of section header string table is invalid, expect "
                                     + std::to_string(SHT_STRTAB)
                                     + ", current:" + std::to_string(shdr.sh_type));
        }

        struct stat statBuf{};
        if (::fstat(fd, &statBuf) == -1) {
            throw std::runtime_error("failed to stat " + path.string() + ": " + ::strerror(errno));
        }
        if (statBuf.st_size < 0) {
            throw std::runtime_error("invalid file size of " + path.string());
        }

        const auto fileSize = static_cast<std::uintmax_t>(statBuf.st_size);
        if (shdr.sh_offset > fileSize || shdr.sh_size > fileSize - shdr.sh_offset) {
            throw std::runtime_error("section header string table of " + path.string()
                                     + " extends past the end of the file");
        }

        /*
         * Keep one extra NUL byte after the string table.  Section names are
         * read as NUL-terminated strings, and the table itself is not required
         * to end with a NUL, so the sentinel keeps the read bounded.
         */
        std::vector<char> rawData(static_cast<std::size_t>(shdr.sh_size) + 1, '\0');
        bytesRead = ::pread(fd, rawData.data(), shdr.sh_size, shdr.sh_offset);
        if (bytesRead == -1) {
            throw std::runtime_error("failed to read section header string table of" + path.string()
                                     + ": " + ::strerror(errno));
        }
        if (bytesRead != static_cast<ssize_t>(shdr.sh_size)) {
            throw std::runtime_error("incomplete read section header string table of"
                                     + path.string());
        }

        this->fd = fd;
        this->header = elfHeader;
        this->rawSectionNames = std::move(rawData);
        fd = -1;
    }

    [[nodiscard]] const ElfHeader &fileHeader() const noexcept { return header; }

    // NOTE!!: DO NOT CLOSE
    [[nodiscard]] int underlyingFd() const noexcept { return fd; }

    [[nodiscard]] std::filesystem::path absolutePath() const
    {
        auto path = std::filesystem::path("/proc/self/fd/" + std::to_string(fd));
        return std::filesystem::read_symlink(path);
    }

    [[nodiscard]] std::optional<SectionHeader>
    getSectionHeader(const std::string &name) const noexcept
    {
        SectionHeader shdr{};
        auto offset = header.e_shoff;
        const auto *data = rawSectionNames.data();
        // The vector holds one trailing NUL sentinel that is not part of the table.
        const auto namesSize = rawSectionNames.size() - 1;

        for (auto index = 0; index < header.e_shnum; ++index) {
            // Read exactly one section header entry. The file's e_shentsize is
            // validated in the constructor, so section_size is the correct stride.
            auto bytesRead = ::pread(fd, &shdr, section_size, offset);
            if (bytesRead == -1) {
                std::cerr << "failed to read section header of " << name << ": "
                          << ::strerror(errno) << std::endl;
                return std::nullopt;
            }
            if (bytesRead != static_cast<ssize_t>(section_size)) {
                // reached the end of the section header table
                return std::nullopt;
            }

            if (shdr.sh_name >= namesSize) {
                std::cerr << "section name offset " << shdr.sh_name << " is out of range"
                          << std::endl;
                return std::nullopt;
            }

            auto curName = std::string_view(data + shdr.sh_name);
            if (!curName.empty() && curName == name) {
                return shdr;
            }

            offset += header.e_shentsize;
        }

        return std::nullopt;
    }

private:
    constexpr static auto section_size = sizeof(SectionHeader);
    constexpr static auto header_size = sizeof(ElfHeader);

    std::vector<char> rawSectionNames;
    ElfHeader header;
    int fd{ -1 };
};

using native_elf = Elf<machine_type>;
} // namespace lightElf
