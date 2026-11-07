// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "utils.h"

#include <elf.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
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

        std::vector<char> rawData(shdr.sh_size,
                                  '\0'); // NOTE: must reserve enough space before read
        if (::pread(fd, rawData.data(), shdr.sh_size, shdr.sh_offset) == -1) {
            throw std::runtime_error("failed to read section header string table of" + path.string()
                                     + ": " + ::strerror(errno));
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
        // Everything used below comes from the file being inspected: for a .uab the
        // caller is the untrusted bundle itself, so e_shentsize/e_shnum/e_shoff and
        // every sh_name must be treated as attacker controlled input.
        //
        // The section header table is only well formed when each entry is exactly as
        // large as SectionHeader.  A tampered e_shentsize used to be handed straight
        // to pread, so a value larger than sizeof(SectionHeader) wrote past the end of
        // the on-stack `shdr` below and a smaller value left it only partially filled.
        // Reject anything else instead of reading, which keeps the stack object safe.
        if (header.e_shentsize != section_size) {
            return std::nullopt;
        }

        // The table referenced by e_shoff/e_shnum has to fit inside the file, otherwise
        // the entries the loop walks do not exist and reads would return short data.
        if (!sectionTableInsideFile()) {
            return std::nullopt;
        }

        SectionHeader shdr;
        auto offset = header.e_shoff;

        for (auto index = 0; index < header.e_shnum; ++index) {
            // Read exactly one SectionHeader, never header.e_shentsize bytes: the two
            // are equal at this point, but pinning the read to section_size keeps the
            // stack buffer safe even if the validation above is ever reordered.
            const auto bytesRead = ::pread(fd, &shdr, section_size, offset);
            if (bytesRead == -1) {
                throw std::runtime_error("failed to read section header of" + name + ": "
                                         + ::strerror(errno));
            }
            if (bytesRead != static_cast<ssize_t>(section_size)) {
                return std::nullopt;
            }

            // sh_name is an offset into the section-name string table.  A malformed
            // entry is skipped rather than matched, so a single corrupt offset cannot
            // make the string_view scan past the end of rawSectionNames.
            const auto curName = sectionName(shdr);
            if (curName && !curName->empty() && *curName == name) {
                return shdr;
            }

            offset += section_size;
        }

        return std::nullopt;
    }

private:
    constexpr static auto section_size = sizeof(SectionHeader);

    // Returns the size of the file that fd refers to, or std::nullopt when it cannot be
    // determined.  Needed to prove the section header table lies inside the file before
    // any of it is read.
    [[nodiscard]] std::optional<std::uintmax_t> fileSize() const noexcept
    {
        struct stat st{};
        if (::fstat(fd, &st) == -1) {
            return std::nullopt;
        }

        return static_cast<std::uintmax_t>(st.st_size);
    }

    // Checks e_shoff + e_shnum * sizeof(SectionHeader) <= file size without letting the
    // untrusted e_shoff/e_shnum overflow the computation.
    [[nodiscard]] bool sectionTableInsideFile() const noexcept
    {
        const auto size = fileSize();
        if (!size) {
            return false;
        }

        const auto tableOffset = static_cast<std::uint64_t>(header.e_shoff);
        if (tableOffset > *size) {
            return false;
        }

        const auto tableSize = static_cast<std::uint64_t>(header.e_shnum) * section_size;
        return tableSize <= *size - tableOffset;
    }

    // Resolves sh_name into a bounded view of the section-name string table.  The view
    // must not be built from a raw pointer into the table: an out-of-range sh_name or a
    // table that is not NUL terminated at sh_name would make the length scan read past
    // rawSectionNames.  Returns std::nullopt for such malformed entries.
    [[nodiscard]] std::optional<std::string_view>
    sectionName(const SectionHeader &shdr) const noexcept
    {
        if (shdr.sh_name >= rawSectionNames.size()) {
            return std::nullopt;
        }

        const auto *begin = rawSectionNames.data() + shdr.sh_name;
        const auto remaining = rawSectionNames.size() - shdr.sh_name;
        const auto *terminator = static_cast<const char *>(std::memchr(begin, '\0', remaining));
        if (terminator == nullptr) {
            return std::nullopt;
        }

        return std::string_view{ begin, static_cast<std::size_t>(terminator - begin) };
    }

    constexpr static auto header_size = sizeof(ElfHeader);

    std::vector<char> rawSectionNames;
    ElfHeader header;
    int fd{ -1 };
};

using native_elf = Elf<machine_type>;
} // namespace lightElf
