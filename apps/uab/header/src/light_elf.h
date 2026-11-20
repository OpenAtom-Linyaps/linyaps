// SPDX-FileCopyrightText: 2024-2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "utils.h"

#include <elf.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

        // Always read a whole ElfHeader: sizeof(ElfHeader) happens to equal
        // sizeof(SectionHeader) for 64-bit ELFs, but not for 32-bit files (52 vs 40).
        // The previous code used the section header size here, which left
        // e_shentsize/e_shnum/e_shstrndx uninitialized for 32-bit files.
        ElfHeader elfHeader;
        auto bytesRead = ::pread(fd, &elfHeader, header_size, 0);
        if (bytesRead == -1) {
            throw std::runtime_error("failed to read " + path.string() + ": " + ::strerror(errno));
        }
        if (bytesRead != static_cast<ssize_t>(header_size)) {
            throw std::runtime_error("incomplete read header from " + path.string());
        }

        if (elfHeader.e_ident[EI_MAG0] != ELFMAG0 || elfHeader.e_ident[EI_MAG1] != ELFMAG1
            || elfHeader.e_ident[EI_MAG2] != ELFMAG2 || elfHeader.e_ident[EI_MAG3] != ELFMAG3) {
            throw std::runtime_error(path.string() + "is not an elf file");
        }

        // The ELF ABI requires e_shentsize to be exactly the size of a section header
        // for this ELF class. Both the table walk below and getSectionHeader() use it as
        // the entry stride and as the pread() length, so an attacker-controlled value
        // larger than SectionHeader would overflow the stack object while a smaller one
        // would yield partially initialized headers.
        if (elfHeader.e_shentsize != section_size) {
            throw std::runtime_error("invalid section header entry size of " + path.string()
                                     + ", expect: " + std::to_string(section_size)
                                     + ", current: " + std::to_string(elfHeader.e_shentsize));
        }

        if (elfHeader.e_shoff == 0) {
            throw std::runtime_error("current elf does not has section header table");
        }

        // e_shnum == 0 is the ELF escape hatch for more than SHN_LORESERVE sections, where
        // the real count is stored in sh_size of section 0. This parser does not implement
        // that, so reject the file instead of silently treating it as having no sections.
        if (elfHeader.e_shnum == 0) {
            throw std::runtime_error("current elf has an empty section header table");
        }

        // Every offset below is attacker controlled, so make sure the complete section
        // header table lives inside the file before any entry is dereferenced.
        struct stat st{};
        if (::fstat(fd, &st) == -1) {
            throw std::runtime_error("failed to stat " + path.string() + ": " + ::strerror(errno));
        }
        const auto fileSize = static_cast<std::uint64_t>(st.st_size);
        if (elfHeader.e_shoff > fileSize
            || static_cast<std::uint64_t>(elfHeader.e_shnum) * elfHeader.e_shentsize
              > fileSize - elfHeader.e_shoff) {
            throw std::runtime_error("section header table of " + path.string()
                                     + " is outside of the file");
        }

        // The extended index is a 32-bit sh_link, not the 16-bit e_shstrndx, so keep the
        // wider type when promoting it.
        std::uint32_t shdrstrndx = elfHeader.e_shstrndx;
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
            if (bytesRead != static_cast<ssize_t>(section_size)) {
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

        // The resolved index (either e_shstrndx or the extended sh_link) must point at an
        // entry of the section header table, otherwise shdrstrtab below would be derived
        // from attacker controlled data outside of the table we just validated.
        if (shdrstrndx >= elfHeader.e_shnum) {
            throw std::runtime_error("section header string table index of " + path.string()
                                     + " is out of range");
        }

        auto shdrstrtab = elfHeader.e_shoff + (shdrstrndx * elfHeader.e_shentsize);
        bytesRead = ::pread(fd, &shdr, section_size, shdrstrtab);
        if (bytesRead == -1) {
            throw std::runtime_error("failed to read section header string table of" + path.string()
                                     + ": " + ::strerror(errno));
        }
        if (bytesRead != static_cast<ssize_t>(section_size)) {
            throw std::runtime_error("incomplete read section header string table of"
                                     + path.string());
        }

        if (shdr.sh_type != SHT_STRTAB) {
            throw std::runtime_error("the type of section header string table is invalid, expect "
                                     + std::to_string(SHT_STRTAB)
                                     + ", current:" + std::to_string(shdr.sh_type));
        }

        // The string table range must be inside the file as well, otherwise pread() below
        // would be handed an out-of-range window. A short read used to be silently
        // accepted, which left uninitialized bytes that getSectionHeader() then walked as
        // NUL-terminated C strings.
        if (shdr.sh_offset > fileSize
            || static_cast<std::uint64_t>(shdr.sh_size) > fileSize - shdr.sh_offset) {
            throw std::runtime_error("section header string table of " + path.string()
                                     + " is outside of the file");
        }

        std::vector<char> rawData(shdr.sh_size,
                                  '\0'); // NOTE: must reserve enough space before read
        auto strtabBytes = ::pread(fd, rawData.data(), shdr.sh_size, shdr.sh_offset);
        if (strtabBytes == -1) {
            throw std::runtime_error("failed to read section header string table of" + path.string()
                                     + ": " + ::strerror(errno));
        }
        if (static_cast<std::uint64_t>(strtabBytes) != shdr.sh_size) {
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

    [[nodiscard]] std::optional<SectionHeader> getSectionHeader(const std::string &name) const
    {
        SectionHeader shdr;
        auto offset = header.e_shoff;
        const auto *data = rawSectionNames.data();
        const auto dataSize = rawSectionNames.size();

        for (auto index = 0; index < header.e_shnum; ++index) {
            // The constructor guarantees e_shentsize == sizeof(SectionHeader) and that the
            // entire table is inside the file, so always read a full SectionHeader instead
            // of letting a crafted e_shentsize decide how many bytes are written here.
            auto bytesRead = ::pread(fd, &shdr, section_size, offset);
            if (bytesRead == -1) {
                throw std::runtime_error("failed to read section header of" + name + ": "
                                         + ::strerror(errno));
            }
            if (bytesRead != static_cast<ssize_t>(section_size)) {
                throw std::runtime_error("incomplete read section header of" + name);
            }

            // sh_name is an offset into the section header string table and a malformed
            // file can point it anywhere, so skip entries outside of the table and bound
            // the name to the table size: a missing terminating NUL must not make
            // string_view walk past the end of the buffer.
            if (shdr.sh_name < dataSize) {
                const auto *begin = data + shdr.sh_name;
                const auto *end = std::find(begin, data + dataSize, '\0');
                auto curName = std::string_view(begin, static_cast<std::size_t>(end - begin));
                if (!curName.empty() && curName == name) {
                    return shdr;
                }
            }

            offset += section_size;
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
