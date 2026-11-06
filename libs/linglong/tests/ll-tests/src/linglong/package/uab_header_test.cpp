/*
 * SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "light_elf.h"

#include <common/tempdir.h>

#include <elf.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Elf64 = lightElf::Elf<64>;

struct ElfImage
{
    std::uint16_t sectionHeaderEntrySize{ sizeof(Elf64_Shdr) };
    std::uint16_t sectionCount{ 0 };
    std::vector<Elf64_Shdr> headers;
    std::string stringTable;

    std::uint32_t stringTableIndex{ 1 };
};

// Writes a minimal ELF64 image whose section header table starts right after
// the ELF header and whose section name table follows the headers.  Only the
// second section header is completed for the string table itself.
std::filesystem::path writeElfImage(const TempDir &dir,
                                    const std::string &name,
                                    ElfImage image)
{
    constexpr std::size_t elfHeaderSize = sizeof(Elf64_Ehdr);

    const auto tableSize = image.headers.size() * sizeof(Elf64_Shdr);
    const auto stringTableOffset = elfHeaderSize + tableSize;

    if (image.headers.size() > image.stringTableIndex) {
        auto &stringTableHeader = image.headers[image.stringTableIndex];
        stringTableHeader.sh_name = 1;
        stringTableHeader.sh_type = SHT_STRTAB;
        stringTableHeader.sh_offset = stringTableOffset;
        stringTableHeader.sh_size = image.stringTable.size();
    }

    Elf64_Ehdr elfHeader{};
    elfHeader.e_ident[EI_MAG0] = ELFMAG0;
    elfHeader.e_ident[EI_MAG1] = ELFMAG1;
    elfHeader.e_ident[EI_MAG2] = ELFMAG2;
    elfHeader.e_ident[EI_MAG3] = ELFMAG3;
    elfHeader.e_ident[EI_CLASS] = ELFCLASS64;
    elfHeader.e_ident[EI_DATA] = ELFDATA2LSB;
    elfHeader.e_ident[EI_VERSION] = EV_CURRENT;
    elfHeader.e_type = ET_EXEC;
    elfHeader.e_machine = EM_X86_64;
    elfHeader.e_version = EV_CURRENT;
    elfHeader.e_shoff = elfHeaderSize;
    elfHeader.e_shentsize = image.sectionHeaderEntrySize;
    elfHeader.e_shnum = static_cast<std::uint16_t>(
      image.sectionCount != 0 ? image.sectionCount : image.headers.size());
    elfHeader.e_shstrndx = image.stringTableIndex;

    std::vector<char> bytes(elfHeaderSize, '\0');
    std::memcpy(bytes.data(), &elfHeader, elfHeaderSize);

    for (const auto &header : image.headers) {
        const auto *raw = reinterpret_cast<const char *>(&header);
        bytes.insert(bytes.end(), raw, raw + sizeof(Elf64_Shdr));
    }

    bytes.insert(bytes.end(), image.stringTable.begin(), image.stringTable.end());

    const auto path = dir.path() / name;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return path;
}

// string table layout:
//   0: '\0'
//   1: ".shstrtab\0"
//  11: ".text\0"
constexpr std::string_view stringTable = std::string_view{ "\0.shstrtab\0.text\0", 17 };

ElfImage validImage()
{
    ElfImage image;
    image.stringTable = std::string{ stringTable };

    Elf64_Shdr nullHeader{};
    Elf64_Shdr stringTableHeader{};
    Elf64_Shdr textHeader{};
    textHeader.sh_name = 11;
    textHeader.sh_type = SHT_PROGBITS;

    image.headers = { nullHeader, stringTableHeader, textHeader };
    return image;
}

TEST(UabHeaderTest, FindsSectionByName)
{
    TempDir dir;
    const auto path = writeElfImage(dir, "valid.uab", validImage());

    Elf64 elf{ path };
    const auto header = elf.getSectionHeader(".text");
    ASSERT_TRUE(header.has_value());
    EXPECT_EQ(header->sh_type, SHT_PROGBITS);
}

TEST(UabHeaderTest, RejectsOversizedSectionHeaderEntrySize)
{
    TempDir dir;
    auto image = validImage();
    image.sectionHeaderEntrySize = sizeof(Elf64_Shdr) * 2;
    const auto path = writeElfImage(dir, "oversized-shentsize.uab", image);

    EXPECT_THROW((void)Elf64{ path }, std::runtime_error);
}

TEST(UabHeaderTest, RejectsUndersizedSectionHeaderEntrySize)
{
    TempDir dir;
    auto image = validImage();
    image.sectionHeaderEntrySize = sizeof(Elf64_Shdr) / 2;
    const auto path = writeElfImage(dir, "undersized-shentsize.uab", image);

    EXPECT_THROW((void)Elf64{ path }, std::runtime_error);
}

TEST(UabHeaderTest, TruncatedSectionHeaderTableReturnsNothing)
{
    TempDir dir;
    auto image = validImage();
    // Claim three entries but only write two; the third read is short.
    image.sectionCount = 3;
    image.headers.resize(2);
    const auto path = writeElfImage(dir, "truncated-table.uab", image);

    Elf64 elf{ path };
    EXPECT_FALSE(elf.getSectionHeader(".text").has_value());
}

TEST(UabHeaderTest, OutOfRangeSectionNameReturnsNothing)
{
    TempDir dir;
    auto image = validImage();
    image.headers[2].sh_name = static_cast<std::uint32_t>(image.stringTable.size() + 16);
    const auto path = writeElfImage(dir, "bad-name-offset.uab", image);

    Elf64 elf{ path };
    EXPECT_FALSE(elf.getSectionHeader(".text").has_value());
}

TEST(UabHeaderTest, UnterminatedStringTableStaysBounded)
{
    TempDir dir;
    auto image = validImage();
    // No trailing NUL: the reader must stop at the string table size, not walk
    // into the heap looking for one.
    image.stringTable = std::string{ "\0.text", 6 };
    image.headers[2].sh_name = 1;
    const auto path = writeElfImage(dir, "unterminated-strtab.uab", image);

    Elf64 elf{ path };
    // A matching name is still readable through the sentinel...
    EXPECT_TRUE(elf.getSectionHeader(".text").has_value());
    // ...and a non-matching lookup must not read past the table.
    EXPECT_FALSE(elf.getSectionHeader(".data").has_value());
}

} // namespace
